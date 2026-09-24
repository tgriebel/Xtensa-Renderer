#include "shaderBinding.h"
#include "bindings.h"
#include "../render_resources/gpuBuffer.h"
#include "../render_state/rhi.h"
#include "../render_resources/gpuImage.h"
#include "../render_resources/imageArray.h"

ShaderBinding::ShaderBinding( const char* name, const bindType_t type, const uint32_t descriptorCount, const bindStateFlag_t flags )
{
	m_state.slot = ~0x00;
	m_state.type = type;
	m_state.maxDescriptorCount = descriptorCount;
	m_state.flags = flags;

	const uint32_t bufSize = 128;

	const uint32_t sizeBytes = static_cast<uint32_t>( strlen( name ) );
	assert( sizeBytes <= bufSize );

	uint8_t hashBytes[ bufSize ];
	memcpy( hashBytes, &m_state, sizeBytes );

	m_hash = Hash( reinterpret_cast<const uint8_t*>( name ), sizeBytes );
	m_name = name;
}


uint32_t ShaderBinding::GetSlot() const
{
	return m_state.slot;
}


bindType_t ShaderBinding::GetType() const
{
	return m_state.type;
}


bool ShaderBinding::IsArrayType() const
{
	return ( GetBindSemantic( m_state.type ) == bindSemantic_t::IMAGE_ARRAY );
}


uint32_t ShaderBinding::GetMaxDescriptorCount() const
{
	return m_state.maxDescriptorCount;
}


bindStateFlag_t ShaderBinding::GetBindFlags() const
{
	return m_state.flags;
}


uint32_t ShaderBinding::GetHash() const
{
	return m_hash;
}

const char* ShaderBinding::GetName() const
{
	return m_name;
}


void ShaderBindSet::Create( const char* name, const ShaderBinding bindings[], const uint32_t bindCount )
{
	if ( bindCount == 0 )
	{
		assert( 0 );
		return;
	}

	{
		m_lifetime = resourceLifeTime_t::REBOOT;
		RenderResource::Create( resourceType_t::BINDSET, m_lifetime );
	}

	m_name = name;

	// Build bind set
	{
		m_bindMap.reserve( bindCount );

		std::vector<uint32_t> hashes;
		hashes.resize( bindCount );

		for ( uint32_t i = 0; i < bindCount; ++i )
		{
			ShaderBinding& binding = m_bindMap[ bindings[ i ].GetHash() ];
			binding = bindings[ i ];
			binding.SetSlot( i );
			hashes[ i ] = binding.GetHash();
		}

		m_hash = Hash( reinterpret_cast<const uint8_t*>( hashes.data() ), bindCount * sizeof( hashes[ 0 ] ) );
		m_valid = false;
	}

	// Create API object
#ifdef USE_VULKAN
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
	layoutBindings.resize( bindCount );

	for ( uint32_t i = 0; i < bindCount; ++i )
	{
		const ShaderBinding* binding = GetBinding( i );

		layoutBindings[ i ] = {};
		layoutBindings[ i ].binding = binding->GetSlot();
		layoutBindings[ i ].descriptorCount = binding->GetMaxDescriptorCount();
		layoutBindings[ i ].descriptorType = vk_GetDescriptorType( binding->GetType() );
		layoutBindings[ i ].stageFlags = vk_GetStageFlags( binding->GetBindFlags() );
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>( layoutBindings.size() );
	layoutInfo.pBindings = layoutBindings.data();

	VK_CHECK_RESULT( vkCreateDescriptorSetLayout( context.device, &layoutInfo, nullptr, &vk_layout ) );

	vk_SetObjectName( (uint64_t)vk_layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, vk_BuildObjectName( "DescriptorSetLayout", m_name.c_str() ).c_str() );
#else
	assert(0);
#endif

	m_valid = true;
}


void ShaderBindSet::Destroy()
{
	if( m_valid == false ) {
		return;
	}
#ifdef USE_VULKAN
	vkDestroyDescriptorSetLayout( context.device, vk_layout, nullptr );
	vk_layout = VK_NULL_HANDLE;
#endif
}


const uint32_t ShaderBindSet::Count() const
{
	return static_cast<uint32_t>( m_bindMap.size() );
}


const uint32_t ShaderBindSet::GetHash() const
{
	return m_hash;
}


const char* ShaderBindSet::GetName() const
{
	return m_name.c_str();
}


const ShaderBinding* ShaderBindSet::GetBinding( const uint32_t id ) const
{
	auto it = m_bindMap.begin();
	std::advance( it, id );

	if ( it != m_bindMap.end() ) {
		return &it->second;
	}
	return nullptr;
}


bool ShaderBindSet::HasBinding( const uint32_t id ) const
{
	return ( GetBinding( id ) != nullptr );
}


bool ShaderBindSet::HasBinding( const ShaderBinding& binding ) const
{
	const uint32_t hash = binding.GetHash();
	auto it = m_bindMap.find( hash );
	if ( it != m_bindMap.end() ) {
		return true;
	}
	return false;
}


#ifdef USE_VULKAN
VkDescriptorSet ShaderBindParms::GetVkObject() const
{
	return vk_descriptorSets[ context.bufferId ];
}


void ShaderBindParms::SetVkObject( const VkDescriptorSet descSet[ MaxFrameStates ] )
{
	for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx )
	{
		assert( descSet[ frameIx ] != VK_NULL_HANDLE );
		vk_descriptorSets[ frameIx ] = descSet[ frameIx ];
	}
}


void ShaderBindParms::InitApiObjects()
{
	for ( uint32_t i = 0; i < MaxFrameStates; ++i ) {
		vk_descriptorSets[ i ] = VK_NULL_HANDLE;
	}
}
#endif


bool ShaderBindParms::IsValid() const
{
	return ( static_cast<uint32_t>( m_attachments[ context.bufferId ].size() ) == m_bindSet->Count() );
}


bool ShaderBindParms::AttachmentChanged( const ShaderBinding& binding ) const
{
	const uint32_t hash = binding.GetHash();
	auto it = m_dirty[ context.bufferId ].find( hash );
	if( it != m_dirty[ context.bufferId ].end() ) {
		return it->second;
	}
	return false;
}


void ShaderBindParms::Clear()
{
	for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx )
	{
		m_attachments[ frameIx ].clear();
		m_dirty[ frameIx ].clear();
	}
}


void ShaderBindParms::Bind( const ShaderBinding& binding, const ShaderAttachment attachment )
{
	if( m_bindSet->HasBinding( binding ) )
	{
		if( binding.IsArrayType() ) {
			assert( attachment.GetImageArray()->Count() <= binding.GetMaxDescriptorCount() );
		}
		assert( GetBindSemantic( binding.GetType() ) == attachment.GetSemantic() );

		const uint32_t hash = binding.GetHash();
		m_dirty[ context.bufferId ][ hash ] = ( m_attachments[ context.bufferId ][ hash ] != attachment );
		m_attachments[ context.bufferId ][ hash ] = attachment;
	} else {
		assert( 0 );
	}
}


void ShaderBindParms::Unbind( const ShaderBinding& binding )
{
	if ( m_bindSet->HasBinding( binding ) )
	{
		const uint32_t hash = binding.GetHash();
		m_dirty[ context.bufferId ][ hash ] = true;
		m_attachments[ context.bufferId ][ hash ] = ShaderAttachment();
	} else {
		assert( 0 );
	}
}


std::string ShaderBindParms::AsString() const
{
	auto& frameAttachments = m_attachments[ context.bufferId ];

	std::stringstream ss;

	ss << m_bindSet->GetName() << ": " << m_entryId << "\n{\n";

	for ( uint32_t bindSlot = 0; bindSlot < m_bindSet->Count(); ++bindSlot )
	{
		const ShaderBinding* bind = m_bindSet->GetBinding( bindSlot );

		ss << "\t" << bind->GetSlot() << ": {\"" << bind->GetName();
		ss << "\" : " << s_bindTypeName[ (uint32_t)bind->GetType() ] << "}, ";

		const ShaderAttachment* attachment = GetAttachment( *bind );

		if ( attachment == nullptr )
		{
			ss << "\n";
			continue;
		}

		switch ( attachment->GetSemantic() )
		{
			case bindSemantic_t::BUFFER:
			{
				ss << "\"" << attachment->GetBuffer()->GetName() << "\"";
			}
			break;

			case bindSemantic_t::IMAGE:
			{
				ss << "\"" << attachment->GetImage()->gpuImage->GetDebugName() << "\"";
			}
			break;

			case bindSemantic_t::IMAGE_ARRAY:
			{
				ss << "{";
				const ImageArray* imageArray = attachment->GetImageArray();

				const uint32_t arrayCount = imageArray->Count();
				const uint32_t displayCount = Min( 4u, arrayCount );

				for ( uint32_t imageIndex = 0; imageIndex < displayCount; ++imageIndex )
				{
					const Image* image = (*imageArray)[ imageIndex ];
					ss << "\"" << ( image->gpuImage ? image->gpuImage->GetDebugName() : "NULL" ) << "\"";
					
					if( imageIndex < ( imageArray->Count() - 1) ) {
						ss << ", ";
					}
				}
				if( arrayCount > displayCount ) {
					ss << "...";
				}
				ss << "}";
			}
			break;

			case bindSemantic_t::IMAGE_SAMPLER:
			{
				ss << "{SAMPLER_FORMAT_NOT_IMPLEMENTED}";
			}
			break;
		}
		ss << "\n";
	}

	ss << "}";

	return ss.str();
}


const ShaderAttachment* ShaderBindParms::GetAttachment( const ShaderBinding& binding ) const
{
	auto it = m_attachments[ context.bufferId ].find( binding.GetHash() );
	if ( it != m_attachments[ context.bufferId ].end() ) {
		return &it->second;
	}
	return nullptr;
}


const ShaderAttachment* ShaderBindParms::GetAttachment( const uint32_t id ) const
{
	auto it = m_attachments[ context.bufferId ].begin();
	std::advance( it, id );

	if ( it != m_attachments[ context.bufferId ].end() ) {
		return &it->second;
	}
	return nullptr;
}
