#include "vertexInput.h"

#define SHADER_STRUCTS_CPP
#include "../../shaders/gpuShared.h"

VertexDescription GetVertexAttributeDescriptions()
{
	uint32_t attribId = 0;

	VertexDescription attributeDescriptions;
	attributeDescriptions.Resize( MaxVertexAttribs );

	attributeDescriptions[ attribId ].desc.binding = 0;
	attributeDescriptions[ attribId ].desc.location = attribId;
	attributeDescriptions[ attribId ].desc.format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].desc.offset = offsetof( vsInput_t, inPosition );
	++attribId;

	attributeDescriptions[ attribId ].desc.binding = 0;
	attributeDescriptions[ attribId ].desc.location = attribId;
	attributeDescriptions[ attribId ].desc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[ attribId ].desc.offset = offsetof( vsInput_t, inColor );
	++attribId;

	attributeDescriptions[ attribId ].desc.binding = 0;
	attributeDescriptions[ attribId ].desc.location = attribId;
	attributeDescriptions[ attribId ].desc.format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].desc.offset = offsetof( vsInput_t, inNormal );
	++attribId;

	attributeDescriptions[ attribId ].desc.binding = 0;
	attributeDescriptions[ attribId ].desc.location = attribId;
	attributeDescriptions[ attribId ].desc.format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].desc.offset = offsetof( vsInput_t, inTangent );
	++attribId;

	attributeDescriptions[ attribId ].desc.binding = 0;
	attributeDescriptions[ attribId ].desc.location = attribId;
	attributeDescriptions[ attribId ].desc.format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].desc.offset = offsetof( vsInput_t, inBitangent );
	++attribId;

	attributeDescriptions[ attribId ].desc.binding = 0;
	attributeDescriptions[ attribId ].desc.location = attribId;
	attributeDescriptions[ attribId ].desc.format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[ attribId ].desc.offset = offsetof( vsInput_t, uv0 );
	++attribId;

	attributeDescriptions[ attribId ].desc.binding = 0;
	attributeDescriptions[ attribId ].desc.location = attribId;
	attributeDescriptions[ attribId ].desc.format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[ attribId ].desc.offset = offsetof( vsInput_t, uv1 );
	++attribId;

	assert( attribId == MaxVertexAttribs );

	return attributeDescriptions;
}