#include "MCMesher.h"

MCMesher::MCMesher(float intervalStart, float intervalEnd, int gridSize, float isoValue)
    : intervalStart(intervalStart), intervalEnd(intervalEnd), gridSize(gridSize), isoValue(isoValue) {
    if (gridSize <= 0)
        throw std::invalid_argument("gridSize must be > 0");
    stepSize = (intervalEnd - intervalStart) / static_cast<float>(gridSize);
}

std::vector<glm::vec3> MCMesher::marchingCubes(const SDF& sdf) {

    std::vector<glm::vec3> meshVertices;

    float x0 = intervalStart;

    for (int i{0}; i < gridSize; ++i) {
        float x1 = intervalStart + (i + 1) * stepSize;
        float y0 = intervalStart;

        for (int j{0}; j < gridSize; ++j) {
            float y1 = intervalStart + (j + 1) * stepSize;
            float z0 = intervalStart;

            for (int k{0}; k < gridSize; ++k) {
                float z1 = intervalStart + (k + 1) * stepSize;
                
                for (const glm::vec3 &vertex: getCellMeshVertices(x0, y0, z0, x1, y1, z1, sdf)) {
                    meshVertices.push_back(vertex);
                }

                z0 = z1; // Set new starting points to end points without recalculation
            }

            y0 = y1;
        }

        x0 = x1;
    }

    return meshVertices;
}

std::vector<glm::vec3> MCMesher::getCellMeshVertices(float x0, float y0, float z0, float x1,
        float y1, float z1, const SDF &sdf) {
    const std::array<glm::vec3, 8> vertices = buildCellVertices(x0, y0, z0, x1, y1, z1);
    const std::array<float, 8> samples = sampleGrid(vertices, sdf);
    int cubeLookupIndex = getLookupIndex(samples);
    std::array<glm::vec3, 12> intersections = getIntersections(cubeLookupIndex, vertices, samples);

    std::vector<glm::vec3> cellMeshVertices;

    cellMeshVertices.reserve(12);

    const int *row = triangleTable[cubeLookupIndex];
    
    for (int i = 0; i < 12 && row[i] != -1; i += 3) {
        const int e0 = row[i + 0];
        const int e1 = row[i + 1];
        const int e2 = row[i + 2];

        cellMeshVertices.emplace_back(intersections[e0]);
        cellMeshVertices.emplace_back(intersections[e1]);
        cellMeshVertices.emplace_back(intersections[e2]);
    }

    return cellMeshVertices;
}

std::array<glm::vec3, 8> MCMesher::buildCellVertices(float x0, float y0, float z0, float x1,
        float y1, float z1) {
    std::array<glm::vec3, 8> cellVertices;

    cellVertices[0] = glm::vec3(x0, y0, z0);
    cellVertices[1] = glm::vec3(x1, y0, z0);
    cellVertices[2] = glm::vec3(x0, y1, z0);
    cellVertices[3] = glm::vec3(x1, y1, z0);
    cellVertices[4] = glm::vec3(x0, y0, z1);
    cellVertices[5] = glm::vec3(x1, y0, z1);
    cellVertices[6] = glm::vec3(x0, y1, z1);
    cellVertices[7] = glm::vec3(x1, y1, z1);

    return cellVertices;
}

std::array<float, 8> MCMesher::sampleGrid(const std::array<glm::vec3, 8> &vertices,
        const SDF &sdf) {
    std::array<float, 8> samples;

    for (size_t i{0}; i < 8; ++i) {
        const glm::vec3 &vertex = vertices[i];
        samples[i] = sdf(vertex.x, vertex.y, vertex.z);
    }

    return samples;
}

int MCMesher::getLookupIndex(const std::array<float, 8> &samples) {
    int lookupIndex = 0;

    for (int i = 0; i < 8; i++)
        if (samples[i] < isoValue)
            lookupIndex |= (1 << i);
    return lookupIndex;
}

glm::vec3 MCMesher::interpolate(const glm::vec3& p0, const glm::vec3& p1, float v0,
                                float v1) const {
    float t = (isoValue - v0) / (v1 - v0);
    return p0 + t * (p1 - p0);
}

std::array<glm::vec3, 12> MCMesher::getIntersections(int cubeLookupIndex,
                                                     const std::array<glm::vec3, 8>& vertices,
                                                     const std::array<float, 8>& samples) {
    std::array<glm::vec3, 12> intersections{};

    int intersectionKey = edgeTable[cubeLookupIndex];
    int intersectionIndex = 0;

    while (intersectionKey) {
        if (intersectionKey & 1) {
            int vertexIndex0 = edgeVertexIndices[intersectionIndex][0];
            int vertexIndex1 = edgeVertexIndices[intersectionIndex][1];

            glm::vec3 intersection = interpolate(vertices[vertexIndex0], vertices[vertexIndex1],
                samples[vertexIndex0], samples[vertexIndex1]);
            intersections[intersectionIndex] = intersection;
        }

        intersectionIndex++;
        intersectionKey >>= 1;
    }

    return intersections;
}

/* TEMP
void MCMesher::tempWriteToObj(const std::string &path, const std::vector<glm::vec3> &vertices) {
    assert(!vertices.empty() && vertices.size() % 3 == 0);

    std::ofstream out(path);
    assert(out);

    out << "# simple triangle-soup OBJ\n" << "o Mesh\n";
    out << std::setprecision(7) << std::fixed;

    for (const auto &v: vertices)
        out << "v " << v.x << ' ' << v.y << ' ' << v.z << '\n';

    const size_t triCount = vertices.size() / 3;

    for (size_t t = 0; t < triCount; ++t) {
        const size_t i = 3 * t + 1;
        out << "f " << i << ' ' << (i + 1) << ' ' << (i + 2) << '\n';
    }
}

float MCMesher::sdfUnitSphere(float x, float y, float z) { return std::sqrt(x * x + y * y + z * z) - 1; }
*/
