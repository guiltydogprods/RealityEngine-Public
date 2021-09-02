/*
 *  VCache.cpp
 *  ionEngine
 *
 *  Created by Claire Rogers on 13/02/2009.
 *  Copyright 2009 Spinning Head Software. All rights reserved.
 *
 */

#include "stdafx.h"
#include "VCache.h"

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>

//#include "ion/Gfx/IndexBuffer.h"

const float FindVertexScore_CacheDecayPower = 1.5f;
const float FindVertexScore_LastTriScore = 0.75f;
const float FindVertexScore_ValenceBoostScale = 2.0f;
const float FindVertexScore_ValenceBoostPower = 0.5f;

float FindVertexScore(VCacheVertex* vertexData)
{
	if (vertexData->m_numActiveTris == 0)
	{
		// No tri needs this vertex!
		return -1.0f;
	}
	
	float Score = 0.0f;
	int8_t CachePosition = vertexData->m_cacheTag;
	if ( CachePosition < 0 )
	{
		// Vertex is not in FIFO cache - no score.
	}
	else
	{
		if ( CachePosition < 3 )
		{
			// This vertex was used in the last triangle,
			// so it has a fixed score, whichever of the three
			// it's in. Otherwise, you can get very different
			// answers depending on whether you add
			// the triangle 1,2,3 or 3,1,2 - which is silly.
			Score = FindVertexScore_LastTriScore;
		}
		else
		{
//			printf("Max Cache Size: %d\n", kMaxCacheSize);
			if (CachePosition >= kMaxCacheSize)
			{
				printf("Warning fallen out of cache\n");
			}
//			assert (CachePosition < kMaxCacheSize);
			// Points for being high in the cache.
			const float Scaler = 1.0f / ( kMaxCacheSize - 3 );
			Score = 1.0f - ( CachePosition - 3 ) * Scaler;
			Score = powf ( Score, FindVertexScore_CacheDecayPower );
		}
	}
	
	// Bonus points for having a low number of tris still to
	// use the vert, so we get rid of lone verts quickly.
	float ValenceBoost = powf ( vertexData->m_numActiveTris, -FindVertexScore_ValenceBoostPower );
	Score += FindVertexScore_ValenceBoostScale * ValenceBoost;
	
	return Score;
}

VCache::VCache()
{
	m_bInitialised = false;
}

void VCache::Initialise(rengineTools::VertexBuffer* vertexBuffer, reng::Import::IndexBuffer* indexBuffer)
{
	InitialiseCache();
	
	uint32_t numVerts = vertexBuffer->getNumVertices();
	m_vertices = new VCacheVertex [numVerts];
	memset(m_vertices, 0, sizeof(VCacheVertex) * numVerts);
	m_numVerts = numVerts;
	
	uint32_t numTris = indexBuffer->GetNumIndices() / 3;
	m_triangles = new VCacheTriangle [numTris];
	memset(m_triangles, 0, sizeof(VCacheTriangle) * numTris);
	m_numTris = numTris;
	
	m_numOutputIndices = 0;
	
	uint32_t* pIndices = (uint32_t*)indexBuffer->GetData();
	for (int i = 0; i < numTris; ++i)
	{
		m_triangles[i].m_bRendered = false;
		m_triangles[i].m_score = 0.0f;
		m_triangles[i].m_vertices[0] = *(pIndices++);
		m_triangles[i].m_vertices[1] = *(pIndices++);
		m_triangles[i].m_vertices[2] = *(pIndices++);

		for (int j = 0; j < 3; j++)
		{
			uint32_t index = m_triangles[i].m_vertices[j];
			m_vertices[index].m_cacheTag = -1;
			m_vertices[index].m_index = index;
			m_vertices[index].m_numTris++;
		}
	}
	
	for (int i = 0; i < numTris; ++i)
	{
		for (int j = 0; j < 3; j++)
		{
			uint32_t index = m_triangles[i].m_vertices[j];
			if (!m_vertices[index].m_tris)					//Allocate space if not done already
			{
				uint8_t numTris = m_vertices[index].m_numTris;
				m_vertices[index].m_tris = new uint32_t [numTris];
			}
			uint8_t triIndex = m_vertices[index].m_numActiveTris++;
			m_vertices[index].m_tris[triIndex] = i;
		}
	}
}

void VCache::Shutdown()
{
	for (int i = 0; i < m_numVerts; i++)
	{
		if (m_vertices[i].m_tris)
			delete [] m_vertices[i].m_tris;
	}
	delete [] m_vertices;
	m_numVerts = 0;	
	delete [] m_triangles;
	m_numTris = 0;
}

void VCache::OptimiseIndices(reng::Import::IndexBuffer* indexBuffer)
{
	printf("Processing %d triangles:", m_numTris);
	
	InitialiseScores();
	
	int32_t numTriangles = m_numTris;
	
	int32_t count = 0;
	while (numTriangles)
	{
		if (count == 10000)
		{
			printf(".");
			count = 0;
		}
//		printf("Best tri = %4d: score = %.3f\n", m_maxIndex, m_maxScore);
//		printf("\tVerts: %4d, %4d, %4d\n", m_triangles[m_maxIndex].m_vertices[0], m_triangles[m_maxIndex].m_vertices[1], m_triangles[m_maxIndex].m_vertices[2]);
		CacheVertex(m_vertices[m_triangles[m_maxIndex].m_vertices[0]]);
		CacheVertex(m_vertices[m_triangles[m_maxIndex].m_vertices[1]]);
		CacheVertex(m_vertices[m_triangles[m_maxIndex].m_vertices[2]]);
		AdjustValence(m_vertices[m_triangles[m_maxIndex].m_vertices[0]], m_maxIndex);
		AdjustValence(m_vertices[m_triangles[m_maxIndex].m_vertices[1]], m_maxIndex);
		AdjustValence(m_vertices[m_triangles[m_maxIndex].m_vertices[2]], m_maxIndex);
		m_triangles[m_maxIndex].m_bRendered = true;
		numTriangles--;
		count++;
		
		uint32_t* triIndices = (uint32_t*)indexBuffer->GetData();
//		if (m_numOutputIndices < 400)
//		{
		triIndices[m_numOutputIndices++] = m_triangles[m_maxIndex].m_vertices[0];
		triIndices[m_numOutputIndices++] = m_triangles[m_maxIndex].m_vertices[1];
		triIndices[m_numOutputIndices++] = m_triangles[m_maxIndex].m_vertices[2];
//		}

		CleanupCache();
		FindNextBestTriangle();
		if (m_maxIndex < 0)
		{
			InitialiseScores();
		}
	}
	
	for (int i = 0; i < m_numTris; i++)
	{
		VCacheTriangle& tri = m_triangles[i];
		if (!tri.m_bRendered)
		{
			printf("tri not rendered\n");
			for (int j = 0; j < 3; ++j)
			{
				int vIdx = tri.m_vertices[j];
				VCacheVertex& vertex = m_vertices[vIdx];
				printf("Num Active Tris %d\n", vertex.m_numActiveTris);
			}
		}
	}
	printf("\n");	
}

void VCache::OptimiseVertices(rengineTools::VertexBuffer* vertexBuffer, reng::Import::IndexBuffer* indexBuffer)
{
	uint32_t numVertices = vertexBuffer->getNumVertices();
	reng::Import::VertexStream* vertexStreams = vertexBuffer->getVertexStreams();
	uint8_t* vertices = vertexBuffer->getData(); // (uint8_t*)vertexStreams[0].m_data;
	uint32_t stride = vertexStreams[0].m_stride;
	
	uint8_t* newVerts = new uint8_t [numVertices * stride];
//	ionp::FatVert* newVerts = (ionp::FatVert*)malloc(sizeof(ionp::FatVert) * numVertices);
	int* newIndices = new int [numVertices];
	int newPos = 0;
	for (int i = 0; i < numVertices; i++)
	{
		newIndices[i] = -1;
	}
	uint32_t* ptr = (uint32_t*)indexBuffer->GetData();
	uint32_t numTris = indexBuffer->GetNumIndices() / 3;
	
	for (uint32_t i = 0; i < numTris; ++i)
	{
		for (int j = 0; j < 3; j++)
		{
			uint32_t index = *ptr;
			int newIndex = newIndices[index];
			if (newIndex == -1)
			{
				newIndex = newPos++;
				uint8_t* pSrcVert = vertices + index * stride;
				uint8_t* pNewVert = newVerts + newIndex * stride;
				memcpy(pNewVert, pSrcVert, stride);
/*
				newVerts[newIndex].vertex.x = vertices[index].vertex.x;
				newVerts[newIndex].vertex.y = vertices[index].vertex.y;
				newVerts[newIndex].vertex.z = vertices[index].vertex.z;
				newVerts[newIndex].normal.x = vertices[index].normal.x;
				newVerts[newIndex].normal.y = vertices[index].normal.y;
				newVerts[newIndex].normal.z = vertices[index].normal.z;
				newVerts[newIndex].texCoord0.u = vertices[index].texCoord0.u;
				newVerts[newIndex].texCoord0.v = vertices[index].texCoord0.v;
				newVerts[newIndex].texCoord1.u = vertices[index].texCoord1.u;
				newVerts[newIndex].texCoord1.v = vertices[index].texCoord1.v;
*/
				newIndices[index] = newIndex;
			}
			*(ptr++) = newIndex;
		}
	}
	memcpy(vertices, newVerts, stride * numVertices);
	delete [] newIndices;
	delete [] newVerts;
//	free(newVerts);
}



void VCache::InitialiseCache()
{
	for( uint32_t i = 0; i < kMaxCacheSize + 3; ++i)
	{
		m_vertexCache[i].m_vertexIndex = -1;
		m_vertexCache[i].m_cacheTag = i;
	}
}

int SortCacheLess(const void *elem1, const void* elem2)
{
	int8_t elem1Tag = ((const VCacheElement*)elem1)->m_cacheTag;
	int8_t elem2Tag = ((const VCacheElement*)elem2)->m_cacheTag;
	
	return elem1Tag - elem2Tag;
}

void VCache::CacheVertex(VCacheVertex& vertex)
{
	if (vertex.m_cacheTag < 0)
	{
//Not in the Cache
	//Shuffle everything down.
		for (int i = kMaxCacheSize + 2; i-- > 0;)
		{
//			printf("%d: Cache %d\n", i, m_vertexCache[i].m_cacheTag);
			m_vertexCache[i+1].m_vertexIndex = m_vertexCache[i].m_vertexIndex;
			m_vertexCache[i+1].m_cacheTag = m_vertexCache[i].m_cacheTag+1;
			int32_t vertexIndex = m_vertexCache[i+1].m_vertexIndex;
			if (vertexIndex >= 0 && vertexIndex < m_numVerts)
			{
				m_vertices[m_vertexCache[i+1].m_vertexIndex].m_cacheTag = m_vertexCache[i+1].m_cacheTag;
			}
		}
	}
	else
	{
//Already in cache
		if (vertex.m_cacheTag > 0)								//Check if we are the MRU vertex, if so we do nothing as we're already HEAD of the cache
		{
			for (int i = 0; i < vertex.m_cacheTag; i++)
			{
				m_vertexCache[i].m_cacheTag++;					//Increment cacheTags by 1
			}
			m_vertexCache[vertex.m_cacheTag].m_cacheTag = 0;	//Set element we're interested in to cacheTag 0
			qsort(m_vertexCache, vertex.m_cacheTag+1, sizeof(VCacheElement), SortCacheLess);
			for (int i = 0; i < kMaxCacheSize + 3; i++)
			{
				int32_t vertexIndex = m_vertexCache[i].m_vertexIndex;
				if (vertexIndex >= 0 && vertexIndex < m_numVerts)
				{
					m_vertices[m_vertexCache[i].m_vertexIndex].m_cacheTag = m_vertexCache[i].m_cacheTag;
				}
//				m_vertices[m_vertexCache[i].m_vertexIndex].m_cacheTag = m_vertexCache[i].m_cacheTag;
			}
		}
	}
	m_vertexCache[0].m_vertexIndex = vertex.m_index;
	m_vertexCache[0].m_cacheTag = 0;
	vertex.m_cacheTag = 0;

	for (int i = 0; i < kMaxCacheSize; i++)
	{
		int32_t cacheTag = m_vertexCache[i].m_cacheTag;
		int32_t index = m_vertexCache[i].m_vertexIndex;
		if (cacheTag != i)
		{
			printf("Cache Error at index %d\n", i);
		}
		
		if (index >= 0 && cacheTag != m_vertices[index].m_cacheTag)
		{
			VCacheVertex& checkVertex = m_vertices[index];
			if (checkVertex.m_numActiveTris)
				printf("Cache Error at index %d\n", i);
		}
	}

}

void VCache::CleanupCache()
{
	for (int i = kMaxCacheSize; i < kMaxCacheSize + 3; i++)
	{
		int index = m_vertexCache[i].m_vertexIndex;
		if (index >= 0)
		{
			m_vertices[index].m_cacheTag = -1;
			m_vertexCache[i].m_vertexIndex = -1;
		}
	}
}

void VCache::InitialiseScores()
{
	for (int i = 0; i < m_numVerts; i++)
	{
		m_vertices[i].m_score = FindVertexScore(&m_vertices[i]);
		//			printf("Vert %4d: score = %.3f\n", i, m_vertices[i].m_score);
	}
	
	m_maxScore = 0.0f;
	m_maxIndex = -1;
	for (int i = 0; i < m_numTris; i++)
	{
		if (m_triangles[i].m_bRendered)
			continue;
		
		float score = 0.0f;
		int32_t v1 = m_triangles[i].m_vertices[0];
		int32_t v2 = m_triangles[i].m_vertices[1];
		int32_t v3 = m_triangles[i].m_vertices[2];
		score += m_vertices[v1].m_score;
		score += m_vertices[v2].m_score;
		score += m_vertices[v3].m_score;
		m_triangles[i].m_score = score;
		
		if (score > m_maxScore)
		{
			m_maxScore = score;
			m_maxIndex = i;
		}
	}	
}

void VCache::AdjustValence(VCacheVertex& vertex, int32_t tri)
{
	if (vertex.m_numActiveTris == 0)
	{
		printf("Error: Vertex shouldn't be in use\n");
	}

	int numTris = vertex.m_numActiveTris--;
	
	if (numTris > 1)
	{
		int index = -1;
		//find tri index
		for (int i = 0; i < numTris; i++)
		{
			if (tri == vertex.m_tris[i])
			{
				index = i;
				break;
			}
		}
		//Shuffle up remaining triangles
		for (int i = index; i < numTris-1; i++)
		{
//			printf("Index %d: Tri %d <-- %d\n", i, vertex.m_tris[i], vertex.m_tris[i+1]);
			vertex.m_tris[i] = vertex.m_tris[i+1];
		}
		vertex.m_tris[numTris-1] = tri;
	}
	if (vertex.m_numActiveTris == 0)
	{
		if (vertex.m_cacheTag >= 0)
		{
			vertex.m_cacheTag = -1;
		}
	}
}

void VCache::FindNextBestTriangle()
{
	m_maxIndex = -1;
	m_maxScore = 0.0f;
	for (int i = 0; i < kMaxCacheSize; ++i)
	{
		if (m_vertexCache[i].m_vertexIndex < 0)
			continue;
		
		VCacheVertex& vertex = m_vertices[m_vertexCache[i].m_vertexIndex];
		if (vertex.m_numActiveTris)
		{
			for (int j = 0; j < vertex.m_numActiveTris; ++j)
			{
				float totalScore = 0;
				int triIndex = vertex.m_tris[j];
				VCacheTriangle& tri = m_triangles[triIndex];
				for (int k = 0; k < 3; ++k)
				{
					float score = FindVertexScore(&m_vertices[tri.m_vertices[k]]);
					m_vertices[tri.m_vertices[k]].m_score = score;
					
					totalScore += score;
				}
				tri.m_score = totalScore;
				if (totalScore > m_maxScore)
				{
					m_maxScore = totalScore;
					m_maxIndex = triIndex;
				}
			}
		}
	}
	if (m_maxIndex > m_numTris)
	{
		m_maxIndex = -1;
		printf("Tri out of range\n");
		for (int ci = 0; ci < kMaxCacheSize; ++ci)
		{
			int vertexIndex = m_vertexCache[ci].m_vertexIndex;
			if (vertexIndex < 0)
				continue;
			
			VCacheVertex* vertex = m_vertices + vertexIndex;
			for (int cj = 0; cj < vertex->m_numActiveTris; ++cj)
			{
				float totalScore = 0;
				int triIndex = vertex->m_tris[cj];
				VCacheTriangle& tri = m_triangles[triIndex];
				for (int ck = 0; ck < 3; ++ck)
				{
					float score = FindVertexScore(&m_vertices[tri.m_vertices[ck]]);
					m_vertices[tri.m_vertices[ck]].m_score = score;
					totalScore += score;
				}
				tri.m_score = totalScore;
				if (totalScore > m_maxScore)
				{
					m_maxScore = totalScore;
					m_maxIndex = triIndex;
				}
			}
		}
	}
}
