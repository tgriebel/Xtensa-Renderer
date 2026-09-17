import os
import re
import sys
import json
import subprocess
from pathlib import Path
from itertools import combinations
from dataclasses import dataclass, field
from datetime import datetime

# Parses a json file with shader entries. The entries represent shaders bound to a given pipeline (VS + PS, etc.)
# Shaders are tightly coupled this way due to VS/PS input/output linkage, but the compiler works on individual
# files — so this parses the json, deduplicates individual files, and builds a compile command per file.

# Usage:
# python shader_compiler.py scenes/                                    # full build from directory
# python shader_compiler.py scene.json                                 # full build from single json
# python shader_compiler.py simple.ps.hlsl                             # single shader, auto-discover perms
# python shader_compiler.py -p <permutation>,<permutation> simple.ps.hlsl  # single shader, explicit perms


# Config — all paths relative to this script's location
SCRIPT_DIR = Path( __file__ ).resolve().parent
BIN_DIR    = SCRIPT_DIR.parent.parent / "external" / "vulkan" / "Bin"
DXC        = str( BIN_DIR / "dxc.exe" )
SHADER_DIR = str( SCRIPT_DIR.parent / "shaders" ) + os.sep
OUT_DIR    = str( SCRIPT_DIR.parent / "shaders" / "bin" ) + os.sep
LOG_FILE   = str( SCRIPT_DIR / "shader_build.log" )

# Display paths — relative to script folder for clean log output
SHADER_DIR_DISPLAY = ".." + os.sep + "shaders" + os.sep
OUT_DIR_DISPLAY    = ".." + os.sep + "shaders" + os.sep + "bin" + os.sep

FLAG_MAP = {
    "msaa"    : "USE_MSAA",
    "skycube" : "USE_CUBE_SAMPLER",
    "mrt"     : "USE_MRT",
}

TYPE_EXT = {
    "vs"    : "vs.hlsl",
    "ps"    : "ps.hlsl",
    "cs"    : "cs.hlsl",
    "rgen"  : "rgen.hlsl",
    "rmiss" : "rmiss.hlsl",
    "rchit" : "rchit.hlsl",
    "rahit" : "rahit.hlsl",
    "rint"  : "rint.hlsl",
}

TYPE_SUFFIX = {
    "vs.hlsl"    : "VS",
    "ps.hlsl"    : "PS",
    "cs.hlsl"    : "CS",
    "rgen.hlsl"  : "RGEN",
    "rmiss.hlsl" : "RMISS",
    "rchit.hlsl" : "RCHIT",
    "rahit.hlsl" : "RAHIT",
    "rint.hlsl"  : "RINT",
}

PROFILE = {
    "vs.hlsl"    : "vs_6_0",
    "ps.hlsl"    : "ps_6_0",
    "cs.hlsl"    : "cs_6_0",
    "rgen.hlsl"  : "lib_6_3",
    "rmiss.hlsl" : "lib_6_3",
    "rchit.hlsl" : "lib_6_3",
    "rahit.hlsl" : "lib_6_3",
    "rint.hlsl"  : "lib_6_3",
}

# vs/ps/cs have a single named entry point; RT lib_ shaders export all [shader("...")] annotated functions
ENTRY = {
    "vs.hlsl" : "VSMain",
    "ps.hlsl" : "PSMain",
    "cs.hlsl" : "CSMain",
}

RT_EXTS = { "rgen.hlsl", "rmiss.hlsl", "rchit.hlsl", "rahit.hlsl", "rint.hlsl" }


@dataclass( frozen=True )
class ShaderRecord:
    source  : str
    output  : str
    ext     : str
    macros  : tuple = field( default_factory=tuple )
    display : str   = ""     # short display label for logs


# Combines all json shaders from all scenes so that scene-specific shaders are still compiled
def load_all_shaders( directory: Path ) -> dict:
    all_shaders = []
    for json_file in directory.glob( "*.json" ):
        print( f"Loading {json_file.name}" )
        with open( json_file, "r" ) as f:
            try:
                data = json.load( f )
                if "shaders" in data:
                    all_shaders.extend( data[ "shaders" ] )
            except json.JSONDecodeError as e:
                print( f"WARNING: Failed to parse {json_file.name}: {e}" )
    return { "shaders": all_shaders }


def strip_shader_ext( name: str ) -> str:
    for ext in TYPE_SUFFIX:
        if name.endswith( f".{ext}" ):
            return name[ :-len( ext ) - 1 ]
    return name


def parse_shaders( data ) -> list[ ShaderRecord ]:
    records = []
    for shader in data[ "shaders" ]:
        attribs = shader.keys()

        sources = []
        for key in TYPE_EXT:
            if key in attribs:
                sources.append( ( strip_shader_ext( shader[ key ] ), TYPE_EXT[ key ] ) )

        # Collect valid perms
        perms = shader.get( "perms", shader.get( "perm", [] ) )
        if isinstance( perms, str ):
            perms = [ perms ]

        valid_perms = []
        for perm in perms:
            if perm not in FLAG_MAP:
                print( f"WARNING: Unknown perm '{perm}'" )
                continue
            valid_perms.append( perm )

        # Generate power set: (), (a,), (b,), (a,b), (a,c), (a,b,c), ...
        perm_combos = []
        for r in range( 0, len( valid_perms ) + 1 ):
            for combo in combinations( valid_perms, r ):
                perm_combos.append( combo )

        for combo in perm_combos:
            macros      = tuple( FLAG_MAP[ p ] for p in combo )
            perm_suffix = "".join( f"_{p}" for p in combo )

            for stem, ext in sources:
                type_suffix = TYPE_SUFFIX.get( ext, "" )
                source      = f"{stem}.{ext}"
                out_name    = f"{stem}{type_suffix}{perm_suffix}.spv"

                records.append( ShaderRecord(
                    source  = SHADER_DIR + source,
                    output  = OUT_DIR + out_name,
                    ext     = ext,
                    macros  = macros,
                    display = f"{SHADER_DIR_DISPLAY}{source} -> {OUT_DIR_DISPLAY}{out_name}",
                ) )

    return records


def deduplicate( records: list[ ShaderRecord ] ) -> list[ ShaderRecord ]:
    seen   = set()
    result = []
    for record in records:
        if record not in seen:
            seen.add( record )
            result.append( record )
    return result


def build_command( record: ShaderRecord ) -> list[ str ]:
    profile = PROFILE[ record.ext ]
    is_rt   = record.ext in RT_EXTS

    cmd = [
        DXC,
        "-spirv",
        "-T", profile,
        "-fvk-use-gl-layout",
        "-fspv-target-env=vulkan1.2",
        record.source,
        "-Fo", record.output,
        "-Zi",           # debug info
        "-HV", "2021",   # HLSL 2021
        "-I", SHADER_DIR,
        "-WX",           # warnings as errors
    ]

    if is_rt:
        cmd += [ "-fspv-extension=SPV_KHR_ray_tracing" ]
        cmd += [ "-fspv-extension=SPV_EXT_descriptor_indexing" ]
    else:
        cmd += [ "-E", ENTRY[ record.ext ] ]

    for macro in record.macros:
        cmd += [ "-D", macro ]

    return cmd


def compile_single_shader_source( source: str, perms: tuple, macros: tuple, log ) -> bool:
    path = Path( source )

    ext  = path.name.split( ".", 1 )[ 1 ]   # e.g. "ps.hlsl", "rgen.hlsl"
    stem = path.name.split( ".", 1 )[ 0 ]   # e.g. "resolve", "primaryRay"

    perm_suffix = "".join( f"_{p}" for p in perms )
    out_name    = f"{stem}{TYPE_SUFFIX.get( ext, '' )}{perm_suffix}.spv"

    record = ShaderRecord(
        source  = SHADER_DIR + source,
        output  = OUT_DIR + out_name,
        ext     = ext,
        macros  = macros,
        display = f"{SHADER_DIR_DISPLAY}{source} -> {OUT_DIR_DISPLAY}{out_name}",
    )
    return compile_record( record, log )


def compile_record( record: ShaderRecord, log ) -> bool:
    cmd   = build_command( record )
    short = record.display if record.display else f"{record.source} -> {record.output}"
    label = f"{short} [{' '.join( record.macros )}]" if record.macros else short

    log.write( f"Compiling {label}\n" )
    log.write( " ".join( cmd ) + "\n" )

    result = subprocess.run( cmd, capture_output=True, text=True )
    output = result.stdout + result.stderr

    log.write( output + "\n" )

    if result.returncode != 0:
        print( f"ERROR: {label}" )
        print( output )
        return False
    else:
        print( f"OK: {label}" )
        return True


def discover_perms_from_source( source_path: str ) -> list[ str ]:
    """Scan a shader file for #ifdef / #if defined(...) guards that match known FLAG_MAP macros."""
    macro_to_perm = { v: k for k, v in FLAG_MAP.items() }
    ifdef_pat     = re.compile( r'#\s*ifdef\s+(\w+)' )
    defined_pat   = re.compile( r'defined\s*\(\s*(\w+)\s*\)' )
    found         = set()

    try:
        with open( source_path, "r", encoding="utf-8", errors="replace" ) as f:
            for line in f:
                for pat in ( ifdef_pat, defined_pat ):
                    for match in pat.finditer( line ):
                        macro = match.group( 1 )
                        if macro in macro_to_perm:
                            found.add( macro_to_perm[ macro ] )
    except OSError as e:
        print( f"WARNING: Could not read '{source_path}' for perm discovery: {e}" )

    return sorted( found )


def main():
    # Parse -p/--perm flags and positional args from argv
    args      = []
    cli_perms = []
    argv_iter = iter( sys.argv[ 1: ] )
    for arg in argv_iter:
        if arg in ( "-p", "--perm" ):
            perm_str = next( argv_iter, None )
            if perm_str:
                cli_perms.extend( p.strip() for p in perm_str.split( "," ) if p.strip() )
        elif arg.startswith( "--" ):
            pass  # skip unknown flags
        else:
            args.append( arg )

    cli_macros = []
    for perm in cli_perms:
        if perm not in FLAG_MAP:
            print( f"WARNING: Unknown perm '{perm}'" )
            continue
        cli_macros.append( FLAG_MAP[ perm ] )

    if len( args ) < 1:
        print( "Usage: python shader_compiler.py [-p PERM]... <shader_json_or_directory_or_shader_file>" )
        print( f"  -p/--perm: add a permutation ({', '.join( FLAG_MAP.keys() )})" )
        sys.exit( 1 )

    path = Path( args[ 0 ] )

    with open( LOG_FILE, "w" ) as log:
        log.write( f"Shader build {datetime.now()} [DXC]\n" )
        log.write( "================================\n\n" )

        # Single-file mode — detected by known HLSL extension
        if any( path.name.endswith( f".{e}" ) for e in TYPE_SUFFIX ):
            if cli_perms:
                success = compile_single_shader_source( str( path.name ), tuple( cli_perms ), tuple( cli_macros ), log )
                errors  = 0 if success else 1
            else:
                discovered = discover_perms_from_source( SHADER_DIR + path.name )
                if discovered:
                    print( f"Discovered perms: {', '.join( discovered )}" )

                errors = 0
                for r in range( 0, len( discovered ) + 1 ):
                    for combo in combinations( discovered, r ):
                        macros = tuple( FLAG_MAP[ p ] for p in combo )
                        if not compile_single_shader_source( str( path.name ), combo, macros, log ):
                            errors += 1

            summary = f"\nBuild failed with {errors} error(s)." if errors else "\nAll shaders compiled successfully."
            print( summary )
            log.write( summary + "\n" )
            sys.exit( 0 if errors == 0 else 1 )

        # Full build mode
        if path.is_dir():
            data = load_all_shaders( path )
        elif path.is_file():
            with open( path, "r" ) as f:
                data = json.load( f )
        else:
            print( f"ERROR: '{path}' is not a valid file or directory" )
            sys.exit( 1 )

        records = parse_shaders( data )
        records = deduplicate( records )

        errors  = sum( 1 for r in records if not compile_record( r, log ) )
        summary = f"\nBuild failed with {errors} error(s)." if errors else "\nAll shaders compiled successfully."
        print( summary )
        log.write( summary + "\n" )


if __name__ == "__main__":
    main()
