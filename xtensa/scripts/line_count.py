import os
from pathlib import Path

# Counts lines of authored code in this repo + submodules, excluding external/ dependencies,
# build output, and binary/generated assets.
#
# Usage:
#   python line_count.py

SCRIPT_DIR = Path( __file__ ).resolve().parent
REPO_ROOT  = SCRIPT_DIR.parent.parent          # Vulkan-Renderer/
XTENSA     = REPO_ROOT / "xtensa"
SUBMODULES = REPO_ROOT / "submodules"

CODE_EXTENSIONS = { ".cpp", ".h", ".hpp", ".hlsl", ".py" }

# Each entry: ( label, root_dir, exclude_dir_names, recursive )
# exclude_dir_names: any path component matching one of these is skipped entirely.
CATEGORIES = [
	( "xtensa/src",      XTENSA / "src",     set(), True ),
	( "xtensa/shaders",  XTENSA / "shaders", { "bin" }, False ),
	( "xtensa/scenes",   XTENSA / "scenes",  { "libs", "x64" }, True ),
	( "xtensa/scripts",  XTENSA / "scripts", set(), False ),
	( "xtensa/AssetBaker", XTENSA / "AssetBaker", { "x64" }, False ),
	( "xtensa (root)",   XTENSA,             set(), False ),
	( "submodules/GfxCore", SUBMODULES / "GfxCore", { "external", "x64", "Debug", "Debug Driver", "Release", "Test (Debug)" }, True ),
	( "submodules/SysCore", SUBMODULES / "SysCore", { "x64" }, False ),
]


def count_lines( path ):
	try:
		with open( path, "r", encoding="utf-8", errors="ignore" ) as f:
			return sum( 1 for _ in f )
	except OSError:
		return 0


def iter_files( root, excluded_dirs, recursive ):
	if not root.exists():
		return
	if recursive:
		for dirpath, dirnames, filenames in os.walk( root ):
			dirnames[:] = [ d for d in dirnames if d not in excluded_dirs ]
			for name in filenames:
				if Path( name ).suffix.lower() in CODE_EXTENSIONS:
					yield Path( dirpath ) / name
	else:
		for entry in root.iterdir():
			if entry.is_file() and entry.suffix.lower() in CODE_EXTENSIONS:
				yield entry


def main():
	results = []
	for label, root, excluded_dirs, recursive in CATEGORIES:
		total = sum( count_lines( f ) for f in iter_files( root, excluded_dirs, recursive ) )
		results.append( ( label, total ) )

	name_width = max( len( label ) for label, _ in results )

	print( f"{'Category':<{name_width}}  Lines" )
	print( "-" * ( name_width + 8 ) )
	for label, total in results:
		print( f"{label:<{name_width}}  {total:>6}" )

	xtensa_total = sum( t for label, t in results if label.startswith( "xtensa" ) )
	submodule_total = sum( t for label, t in results if label.startswith( "submodules" ) )
	grand_total = xtensa_total + submodule_total

	print( "-" * ( name_width + 8 ) )
	print( f"{'xtensa subtotal':<{name_width}}  {xtensa_total:>6}" )
	print( f"{'submodules subtotal':<{name_width}}  {submodule_total:>6}" )
	print( f"{'GRAND TOTAL':<{name_width}}  {grand_total:>6}" )


if __name__ == "__main__":
	main()
