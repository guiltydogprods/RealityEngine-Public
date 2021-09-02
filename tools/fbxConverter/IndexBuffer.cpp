#include "IndexBuffer.h"

namespace reng
{
	namespace Import
	{
		IndexBuffer::IndexBuffer(uint32_t numIndices, GfxIndexType type, void* indices)
			: m_numIndices(numIndices)
			, m_indexType(type)
			, m_data(nullptr)
		{
		}

		void IndexBuffer::Destroy()
		{
			printf("GLES2IndexBuffer dtor!\n");
		}

		/*
		GL3IndexBuffer::~GL3IndexBuffer()
		{

		}
		*/
		void IndexBuffer::writeIndices(void* indices)
		{
			uint32_t size = 0;
			switch (m_indexType)
			{
			case k32Bit:
				size = sizeof(uint32_t) * m_numIndices;
				break;
			case k16Bit:
				size = sizeof(uint16_t) * m_numIndices;
				break;
			default:
				AssertMsg(1, "Invalid Index Format");
			}
			//			m_data = (uint8_t*)MemMgr::AllocMemory(size); //uint8_t[size];
			m_data = new uint8_t[size];
			memcpy(m_data, indices, size);
		}

		void IndexBuffer::Upload(bool bKeepLocal)
		{
			/*
						uint32_t size = 0;
						switch (m_indexType)
						{
						case k32Bit:
							size = sizeof(uint32_t) * m_numIndices;
							break;
						case k16Bit:
							size = sizeof(uint16_t) * m_numIndices;
							break;
						default:
							AssertMsg(1, "Invalid Index Format");
						}

						ion::GfxDevice::Instance().GenBuffers(1, &m_glIndexBuffer);
						ion::GfxDevice::Instance().BindBuffer(kElementArrayBuffer, m_glIndexBuffer);
						ion::GfxDevice::Instance().BufferData(kElementArrayBuffer, size, m_data, kStaticDraw);
						ion::GfxDevice::Instance().BindBuffer(kElementArrayBuffer, 0);

						if (!bKeepLocal)
						{
							//			delete [] (uint8_t*)m_data;
							m_data = 0;
						}
			*/
		}

		void IndexBuffer::Bind()
		{
			//			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_glIndexBuffer);
			//			GfxDevice::Instance().CheckError();
		}

		void* IndexBuffer::GetData()
		{
			return m_data;
		}

		uint32_t IndexBuffer::GetNumIndices()
		{
			return m_numIndices;
		}

		GfxIndexType IndexBuffer::GetType()
		{
			return m_indexType;
		}
	}
}
