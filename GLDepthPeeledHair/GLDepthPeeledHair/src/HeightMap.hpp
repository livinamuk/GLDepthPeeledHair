#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <stb_image.h>
#include "Types.h"
#include "Common.h"
#include "Util.hpp"

struct QuadTreeNode {
    glm::vec3 aabbMin;
    glm::vec3 aabbMax;
    int level = 0;
    int chunkIndex = -1;
    int children[4];
    bool IsLeaf() { 
        return chunkIndex != -1;
    }
};

struct HeightMapChunk {
    int baseIndex = 0;
    int indexCount = 0;
    int offsetX = 0;
    int offsetY = 0;
    glm::vec3 center = glm::vec3(0);
};

struct HeightMap {

    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;
    std::vector<HeightMapChunk> m_chunks;
    std::vector<AABB> m_chunkAABBs; 
    std::vector<QuadTreeNode> m_quadTreeNodes;
    GLuint m_VAO = 0;
    GLuint m_VBO = 0;
    GLuint m_EBO = 0;
    int m_width = 0;
    int m_depth = 0;
    int m_chunkSize = 0;
    Transform m_transform;
    int m_quadTreeNodesUsed = 0;

    void UpdateQuadTree() {

        // Determine levels
        int heightMapSize = m_width;
        int ratio = heightMapSize / m_chunkSize;
        int levels = static_cast<int>(std::log2(ratio));
        int nodeCount = (std::pow(4, levels + 1) - 1) / 3;

        // Clear any existing nodes
        m_quadTreeNodes.clear();
        m_quadTreeNodes.resize(nodeCount);

        // Configure root node
        QuadTreeNode& rootNode = m_quadTreeNodes[0];
        rootNode.aabbMin = glm::vec3(1e30f);
        rootNode.aabbMax = glm::vec3(-1e30f);
        for (int first = 0, i = 0; i < m_chunks.size(); i++) {
            AABB& aabb = m_chunkAABBs[i];
            rootNode.aabbMin = Util::Vec3Min(rootNode.aabbMin, aabb.GetBoundsMin());
            rootNode.aabbMax = Util::Vec3Max(rootNode.aabbMax, aabb.GetBoundsMax());
        }
        // Create a fixed-size array, hardcoded to work for 256 size heightmap and 32 size chunks
        int stack[85];
        int stackSize = 0;
        stack[stackSize++] = 0; // Add the root node

        // Iteratively build the quadtree
        int currentNodeCount = 1;
        while (stackSize > 0) {
            int nodeIndex = stack[--stackSize];
            QuadTreeNode& node = m_quadTreeNodes[nodeIndex];
            int currentLevel = node.level;

            // Bail if you're at the bottom level
            if (currentLevel >= levels) {
                continue;
            }
            glm::vec3 nodeCenter = (node.aabbMin + node.aabbMax) * 0.5f;

            // Create 4 children for the current node
            for (int j = 0; j < 4; j++) {
                QuadTreeNode& child = m_quadTreeNodes[currentNodeCount];
                child.level = currentLevel + 1;
                child.aabbMin = node.aabbMin;
                child.aabbMax = node.aabbMax;
                if (j & 1) {
                    child.aabbMin.x = nodeCenter.x;
                }
                else {
                    child.aabbMax.x = nodeCenter.x;
                }
                if (j & 2) {
                    child.aabbMin.z = nodeCenter.z;
                }
                else {
                    child.aabbMax.z = nodeCenter.z;
                }
                node.children[j] = currentNodeCount;
                stack[stackSize++] = currentNodeCount;
                currentNodeCount++;
            }
        }
        // Fill the leaf nodes with the chunk indices
        for (int i = 0; i < nodeCount; i++) {
            QuadTreeNode& node = m_quadTreeNodes[i];
            node.chunkIndex = -1;
            glm::vec3 nodeCenter = (node.aabbMin + node.aabbMax) * 0.5f;
            if (node.level == levels) {
                for (int j = 0; j < m_chunkAABBs.size(); j++) {
                    if ((m_chunkAABBs[j].center.x >= node.aabbMin.x && m_chunkAABBs[j].center.x <= node.aabbMax.x) &&
                        (m_chunkAABBs[j].center.y >= node.aabbMin.y && m_chunkAABBs[j].center.y <= node.aabbMax.y) &&
                        (m_chunkAABBs[j].center.z >= node.aabbMin.z && m_chunkAABBs[j].center.z <= node.aabbMax.z)) {
                        node.chunkIndex = j;
                        break;
                    }
                }
            }
        }
        //std::cout << "ratio: " << ratio << "\n";
        //std::cout << "levels: " << levels << "\n";
        //std::cout << "nodeCount: " << nodeCount << "\n";
        //std::cout << "m_chunks.size(): " << m_chunks.size() << "\n";
    }

    void UpdateChunkAABBs() {
        m_chunkAABBs.resize(m_chunks.size());
        for (int i = 0; i < m_chunks.size(); i++) {
            HeightMapChunk& chunk = m_chunks[i];
            glm::vec3 aabbMin = glm::vec3(1e30f);
            glm::vec3 aabbMax = glm::vec3(-1e30f);
            for (int j = chunk.baseIndex; j < chunk.baseIndex + chunk.indexCount; j++) {
                glm::vec3 worldPos = m_transform.to_mat4() * glm::vec4(m_vertices[m_indices[j]].position, 1.0f);
                aabbMin = Util::Vec3Min(aabbMin, worldPos);
                aabbMax = Util::Vec3Max(aabbMax, worldPos);
            }
            m_chunkAABBs[i] = AABB(aabbMin, aabbMax);
        }
    }

    int GetFullRangeIndexCount() {
        if (m_chunks.size()) {
            return m_chunks[0].baseIndex;
        }
        else {
            return m_indices.size();
        }
    }

    void CreateChunks(int chunkSize) {
        m_chunkSize = chunkSize;
        for (int x = 0; x < m_width; x += chunkSize) {
            for (int z = 0; z < m_width; z += chunkSize) {
                std::vector<unsigned int> indices;
                GenerateChunkIndices(x, z, chunkSize, chunkSize, indices);
                HeightMapChunk& chunk = m_chunks.emplace_back();
                chunk.offsetX = x;
                chunk.offsetX = z;
                chunk.baseIndex = m_indices.size();
                chunk.indexCount = indices.size();
                m_indices.insert(m_indices.end(), indices.begin(), indices.end());
            }
        }
    }

    void SetNormalsAndTangentsFromVertices(Vertex* vert0, Vertex* vert1, Vertex* vert2) {
        // Shortcuts for UVs
        glm::vec3& v0 = vert0->position;
        glm::vec3& v1 = vert1->position;
        glm::vec3& v2 = vert2->position;
        glm::vec2& uv0 = vert0->uv;
        glm::vec2& uv1 = vert1->uv;
        glm::vec2& uv2 = vert2->uv;
        // Edges of the triangle : position delta. UV delta
        glm::vec3 deltaPos1 = v1 - v0;
        glm::vec3 deltaPos2 = v2 - v0;
        glm::vec2 deltaUV1 = uv1 - uv0;
        glm::vec2 deltaUV2 = uv2 - uv0;
        float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
        glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
        glm::vec3 bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;
        glm::vec3 normal = glm::normalize(glm::cross(deltaPos1, deltaPos2));
        vert0->normal = normal;
        vert1->normal = normal;
        vert2->normal = normal;
        vert0->tangent = tangent;
        vert1->tangent = tangent;
        vert2->tangent = tangent;
    }

    void Load(const std::string& filepath, float textureRepeat) {
        int channels;
        unsigned char* data = stbi_load(filepath.c_str(), &m_width, &m_depth, &channels, 0);
        if (!data) {
            std::cout << "Failed to load heightmap image\n";
            return;
        }

        // Vertices
        m_vertices.reserve(m_width * m_depth);
        for (int z = 0; z < m_depth; z++) {
            for (int x = 0; x < m_width; x++) {
                int index = (z * m_width + x) * channels;
                unsigned char value = data[index];
                float heightValue = static_cast<float>(value) / 255.0f;
                Vertex& vertex = m_vertices.emplace_back();
                vertex.position = glm::vec3(x, heightValue, z);
                vertex.uv = glm::vec2((float)x / (m_width - 1) * textureRepeat, float(z) / (m_depth - 1) * textureRepeat);
            }
        }
        stbi_image_free(data);

        // Calculate normals
        for (int z = 0; z < m_depth; z++) {
            for (int x = 0; x < m_width; x++) {
                glm::vec3 current = m_vertices[z * m_width + x].position;
                glm::vec3 left = (x > 0) ? m_vertices[z * m_width + (x - 1)].position : current;
                glm::vec3 right = (x < m_width - 1) ? m_vertices[z * m_width + (x + 1)].position : current;
                glm::vec3 down = (z > 0) ? m_vertices[(z - 1) * m_width + x].position : current;
                glm::vec3 up = (z < m_depth - 1) ? m_vertices[(z + 1) * m_width + x].position : current;
                glm::vec3 dx = right - left;
                glm::vec3 dz = up - down;
                glm::vec3 normal = glm::normalize(glm::cross(dz, dx));
                m_vertices[z * m_width + x].normal = normal;
            }
        }
        // Indices
        m_indices.reserve((m_width - 1) * (m_depth - 1) * 2 * 3); // Rough estimate of the size
        for (int z = 0; z < m_depth - 1; ++z) {
            for (int x = 0; x < m_width; ++x) {
                m_indices.push_back(z * m_width + x);
                m_indices.push_back((z + 1) * m_width + x);
            }
            // Degenerate triangles to connect rows
            if (z < m_depth - 2) {
                m_indices.push_back((z + 1) * m_width + (m_width - 1));
                m_indices.push_back((z + 1) * m_width);
            }
        }
        // Calculate tangents
        for (int z = 0; z < m_depth - 1; ++z) {
            for (int x = 0; x < m_width - 1; ++x) {
                Vertex& v0 = m_vertices[z * m_width + x];
                Vertex& v1 = m_vertices[z * m_width + (x + 1)];
                Vertex& v2 = m_vertices[(z + 1) * m_width + x];
                glm::vec3 deltaPos1 = v1.position - v0.position;
                glm::vec3 deltaPos2 = v2.position - v0.position;
                glm::vec2 deltaUV1 = v1.uv - v0.uv;
                glm::vec2 deltaUV2 = v2.uv - v0.uv;
                float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
                glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
                tangent = glm::normalize(tangent);
                v0.tangent = tangent;
                v1.tangent = tangent;
                v2.tangent = tangent;
            }
        }
        // Normalize tangents
        for (Vertex& vertex : m_vertices) {
            vertex.tangent = glm::normalize(vertex.tangent);
        }

        //std::cout << "Heightmap loaded\n";
        //std::cout << "m_vertices: " << m_vertices.size() << "\n";
        //std::cout << "m_indices: " << m_indices.size() << "\n";
        //std::cout << "Width: " << m_width << "\n";
        //std::cout << "Depth: " << m_depth << "\n";
        CreateChunks(32);
        //std::cout << "m_indices: " << m_indices.size() << "\n";
    }

    void GenerateChunkIndices(int beginX, int beginZ, int chunkWidth, int chunkHeight, std::vector<unsigned int>& indices) {
        indices.reserve((chunkWidth * 2 + 2) * (chunkHeight - 1));
        for (int z = 0; z < chunkHeight - 1; ++z) {
            // Connect the start of the strip if not the first row (degenerate vertex)
            if (z > 0) {
                indices.push_back((beginZ + z) * m_width + beginX);
            }
            for (int x = 0; x < chunkWidth; ++x) {
                int top = (beginZ + z) * m_width + (beginX + x);
                int bottom = (beginZ + (z + 1)) * m_width + (beginX + x);

                indices.push_back(top);
                indices.push_back(bottom);
            }
            // Connect the end of the strip to the start of the next row (degenerate vertex)
            if (z < chunkHeight - 2) {
                indices.push_back((beginZ + (z + 1)) * m_width + (beginX + (chunkWidth - 1)));
            }
        }
    }


    void UploadToGPU() {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_EBO);
        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), &m_vertices[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), &m_indices[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
};