#include "globals.h"
#include "visBuffer.h"

PS_LAYOUT_STANDARD( Texture2D )

struct visOutput_t
{
    uint2 vis : SV_Target0;
};

visOutput_t PSMain( vsToPsInterpolators input, uint primitiveId : SV_PrimitiveID )
{
    visOutput_t output;
    output.vis = EncodeVisibility( input.objectId, primitiveId );
    return output;
}
