#pragma once
#include "stdafx.h"
//#include "ion/Gfx/GfxDevice.h"

namespace reng
{
	namespace Import
	{
		enum GfxIndexType
		{
			k16Bit,
			k32Bit
		};

		class IndexBuffer
		{
		public:
			//			static IndexBuffer* Create(uint32_t numIndices, GfxIndexType type, void* indices = 0);
			IndexBuffer(uint32_t numIndices, GfxIndexType type, void* indices = nullptr);
			~IndexBuffer() {}

			void Destroy();
			void writeIndices(void* data);
			void Upload(bool bKeepLocal = false);
			void Bind();

			void* GetData();
			uint32_t		GetNumIndices();
			GfxIndexType	GetType();
		protected:
			//			virtual ~IndexBuffer() {}

			uint32_t		m_numIndices;
			GfxIndexType	m_indexType;
			uint8_t* m_data;
		};
	}
}
