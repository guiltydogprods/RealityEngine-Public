/*
 *  VCache.h
 *  ionEngine
 *
 *  Created by Claire Rogers on 13/02/2009.
 *  Copyright 2009 Spinning Head Software. All rights reserved.
 *
 */

#pragma once

#include <stdint.h>
#include "fbxConverter.h"

const uint8_t kMaxCacheSize = 32;

struct VCacheTriangle
{	
	bool		m_bRendered;
	float		m_score;
	uint32_t	m_vertices[3];
};

struct VCacheVertex
{
	int32_t		m_index;				//Vertex Index
	float		m_score;				//Current Score
	int8_t		m_cacheTag;				//-1 if not in cache
	int8_t		m_numTris;				//Num Tris using this vertex
	int8_t		m_numActiveTris;		//Num Tris using this vertex not drawn
	uint8_t		m_pad;
	uint32_t*	m_tris;	
};

struct VCacheElement
{
	int32_t		m_vertexIndex;
	uint32_t	m_cacheTag;
};

class VCache
{
public:
	VCache();
	void Initialise(rengineTools::VertexBuffer* vertexBuffer, reng::Import::IndexBuffer* indexBuffer);
	void OptimiseIndices(reng::Import::IndexBuffer* indexBuffer);
	void OptimiseVertices(rengineTools::VertexBuffer* vertexBuffer, reng::Import::IndexBuffer* indexBuffer);
	void Shutdown();
private:
	void InitialiseCache();
	void CacheVertex(VCacheVertex& vertex);
	void CleanupCache();
	
	void InitialiseScores();
	void AdjustValence(VCacheVertex& vertex, int32_t tri);
	void FindNextBestTriangle();
	
	bool			m_bInitialised;
	int32_t			m_numVerts;
	int32_t			m_numTris;
	VCacheElement	m_vertexCache[kMaxCacheSize+3];
	VCacheVertex*	m_vertices;
	VCacheTriangle*	m_triangles;
	uint32_t		m_numOutputIndices;
	float			m_maxScore;
	int32_t			m_maxIndex;
};