#include "Mesher.h"

namespace blob {

    Mesher::Mesher(float intervalStart, float intervalEnd, int resolution, float isoValue)
        : intervalStart{intervalStart},
          intervalEnd{intervalEnd},
          resolution{resolution},
          isoValue{isoValue},
          stepSize{(intervalEnd - intervalStart) / static_cast<float>(resolution)} {
        assert(stepSize > 0 && "Invalid MC resolution");
    }

    std::vector<VertexData> Mesher::marchingCubes(const Sdf &sdf, std::size_t estimatedVertexCount) const {
        std::vector<VertexData> meshVertices;

        if (estimatedVertexCount)
            meshVertices.reserve(estimatedVertexCount);

        for (int i{0}; i < resolution - 1; ++i)
            for (int j{0}; j < resolution - 1; ++j)
                for (int k{0}; k < resolution - 1; ++k)
                    for (const VertexData &vertex: getCellMeshVertices(sdf, i, j, k))
                        meshVertices.push_back(vertex);

        return meshVertices;
    }

    std::vector<VertexData> Mesher::getCellMeshVertices(const Sdf &sdf, int i, int j, int k) const {
        const std::array<glm::vec3, 8> vertices = buildCellVertices(i, j, k);
        const std::array<float, 8> samples = sampleGrid(sdf, vertices);
        int cubeLookupIndex = getLookupIndex(samples);
        std::array<IntersectionPoint, 12> intersections = getIntersections(sdf, cubeLookupIndex, vertices, samples);

        std::vector<VertexData> cellMeshVertices;
        cellMeshVertices.reserve(12);

        const int *row = triangleTable[cubeLookupIndex];

        for (int i = 0; i < 12 && row[i] != -1; i += 3) {
            const IntersectionPoint p0 = intersections[row[i + 0]];
            const IntersectionPoint p1 = intersections[row[i + 1]];
            const IntersectionPoint p2 = intersections[row[i + 2]];

            cellMeshVertices.emplace_back(VertexData{glm::vec4(p0.position, 1.f), glm::vec4(p0.normal, 0.f)});
            cellMeshVertices.emplace_back(VertexData{glm::vec4(p1.position, 1.f), glm::vec4(p1.normal, 0.f)});
            cellMeshVertices.emplace_back(VertexData{glm::vec4(p2.position, 1.f), glm::vec4(p2.normal, 0.f)});
        }

        return cellMeshVertices;
    }

    std::array<glm::vec3, 8> Mesher::buildCellVertices(int i, int j, int k) const {
        std::array<glm::vec3, 8> cellVertices;

        float x0 = intervalStart + i * stepSize;
        float x1 = intervalStart + (i + 1) * stepSize;
        float y0 = intervalStart + j * stepSize;
        float y1 = intervalStart + (j + 1) * stepSize;
        float z0 = intervalStart + k * stepSize;
        float z1 = intervalStart + (k + 1) * stepSize;

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

    std::array<float, 8> Mesher::sampleGrid(const Sdf &sdf, const std::array<glm::vec3, 8> &vertices) const {
        std::array<float, 8> samples;

        for (size_t i{0}; i < 8; ++i) {
            const glm::vec3 &vertex = vertices[i];
            samples[i] = sdf.value(vertex);
        }

        return samples;
    }

    int Mesher::getLookupIndex(const std::array<float, 8> &samples) const {
        int lookupIndex = 0;

        for (int i = 0; i < 8; i++)
            if (samples[i] < isoValue)
                lookupIndex |= (1 << i);
        return lookupIndex;
    }

    glm::vec3 Mesher::interpolate(const glm::vec3 &p0, const glm::vec3 &p1, float v0, float v1) const {
        float t = (isoValue - v0) / (v1 - v0);
        return p0 + t * (p1 - p0);
    }

    std::array<Mesher::IntersectionPoint, 12> Mesher::getIntersections(
            const Sdf &sdf, int cubeLookupIndex, const std::array<glm::vec3, 8> &vertices, const std::array<float, 8> &samples
    ) const {
        std::array<IntersectionPoint, 12> intersections{};

        int intersectionKey = edgeTable[cubeLookupIndex];
        int intersectionIndex = 0;

        while (intersectionKey) {
            if (intersectionKey & 1) {
                int vertexIndex0 = edgeVertexIndices[intersectionIndex][0];
                int vertexIndex1 = edgeVertexIndices[intersectionIndex][1];

                glm::vec3 position = interpolate(
                        vertices[vertexIndex0], vertices[vertexIndex1], samples[vertexIndex0], samples[vertexIndex1]
                );

                intersections[intersectionIndex] = {position, calculateNormal(sdf, position)};
            }

            intersectionIndex++;
            intersectionKey >>= 1;
        }

        return intersections;
    }

    glm::vec3 Mesher::calculateNormal(const Sdf &sdf, const glm::vec3 &p) const {
        float dx = sdf.value({p.x + EPS, p.y, p.z}) - sdf.value({p.x - EPS, p.y, p.z});
        float dy = sdf.value({p.x, p.y + EPS, p.z}) - sdf.value({p.x, p.y - EPS, p.z});
        float dz = sdf.value({p.x, p.y, p.z + EPS}) - sdf.value({p.x, p.y, p.z - EPS});
        return glm::normalize(glm::vec3(dx, dy, dz));
    }

    // TEMP for writing to an OBJ file for debugging
    void Mesher::writeToObj(const std::string &path, const std::vector<VertexData> &vertices) const {
        assert(!vertices.empty() && vertices.size() % 3 == 0);

        std::ofstream out(path);
        assert(out);

        out << "# MC Meshed OBJ\n"
            << "o Mesh\n";
        out << std::setprecision(7) << std::fixed;

        for (const auto &v: vertices)
            out << "v " << v.position.x << ' ' << v.position.y << ' ' << v.position.z << '\n';

        for (const auto &v: vertices)
            out << "vn " << v.normal.x << ' ' << v.normal.y << ' ' << v.normal.z << '\n';

        const size_t triCount = vertices.size() / 3;
        for (size_t t = 0; t < triCount; ++t) {
            const size_t i = 3 * t + 1;
            out << "f " << i << "//" << i << ' ' << i + 1 << "//" << i + 1 << ' ' << i + 2 << "//" << i + 2 << '\n';
        }
    }

} // namespace blob
