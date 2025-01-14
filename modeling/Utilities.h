#ifndef MANA_MODELING_UTILITIES_HPP
#define MANA_MODELING_UTILITIES_HPP

#include <osg/Transform>
#include <osg/Geometry>
#include <osg/Camera>

namespace osgVerse
{
    class MeshTopology;

    struct ConvexHull
    {
        std::vector<osg::Vec3> points;
        std::vector<unsigned int> triangles;
    };

    struct Vec3MapComparer
    {
        bool operator()(const osg::Vec3& key1, const osg::Vec3& key2) const
        {
#if 0
            int x0 = round(key1[0] * 1000.0f), x1 = round(key2[0] * 1000.0f);
            int y0 = round(key1[1] * 1000.0f), y1 = round(key2[1] * 1000.0f);
            int z0 = round(key1[2] * 1000.0f), z1 = round(key2[2] * 1000.0f);
            if (x0 < x1) return true; else if (x0 > x1) return false;
            else if (y0 < y1) return true; else if (y0 > y1) return false;
            else return (z0 < z1);
#else
            return key1 < key2;
#endif
        }
    };

    class MeshCollector : public osg::NodeVisitor
    {
    public:
        MeshCollector();
        void setWeldingVertices(bool b) { _weldVertices = b; }
        void setUseGlobalVertices(bool b) { _globalVertices = b; }
        inline void pushMatrix(osg::Matrix& matrix) { _matrixStack.push_back(matrix); }
        inline void popMatrix() { _matrixStack.pop_back(); }

        virtual void reset();
        virtual void apply(osg::Transform& transform);
        virtual void apply(osg::Geode& node);

        virtual void apply(osg::Node& node);
        virtual void apply(osg::Geometry& geometry);

        virtual void apply(osg::Node* n, osg::Drawable* d, osg::StateSet& ss);
        virtual void apply(osg::Node* n, osg::Drawable* d, osg::Texture* ss, int u) {}
        
        enum VertexAttribute { WeightAttr, NormalAttr, ColorAttr, UvAttr };
        std::vector<osg::Vec4>& getAttributes(VertexAttribute a) { return _attributes[a]; }
        const std::vector<osg::Vec3>& getVertices() const { return _vertices; }
        const std::vector<unsigned int>& getTriangles() const { return _indices; }

    protected:
        typedef std::vector<osg::Matrix> MatrixStack;
        MatrixStack _matrixStack;

        std::map<osg::Vec3, unsigned int, Vec3MapComparer> _vertexMap;
        std::map<VertexAttribute, std::vector<osg::Vec4>> _attributes;
        std::vector<osg::Vec3> _vertices;
        std::vector<unsigned int> _indices;
        bool _weldVertices, _globalVertices;
    };

    class BoundingVolumeVisitor : public MeshCollector
    {
    public:
        BoundingVolumeVisitor() : MeshCollector() {}
        
        /** Returned value is in OBB coordinates, using rotation to convert it */
        osg::BoundingBox computeOBB(osg::Quat& rotation, float relativeExtent = 0.1f, int numSamples = 500);
    };
    
    class MeshTopologyVisitor : public MeshCollector
    {
    public:
        MeshTopologyVisitor() : MeshCollector() {}
        virtual void apply(osg::Node* n, osg::Drawable* d, osg::StateSet& ss);

        /** Get topology object */
        MeshTopology* generate();
        osg::StateSet* getMergedStateSet() { return _stateset.get(); }

    protected:
        osg::ref_ptr<osg::StateSet> _stateset;
    };

    /** Create a geometry with specified arrays */
    extern osg::Geometry* createGeometry(osg::Vec3Array* va, osg::Vec3Array* na, osg::Vec2Array* ta,
                                         osg::PrimitiveSet* p, bool autoNormals = true, bool useVBO = true);

    extern osg::Geometry* createGeometry(osg::Vec3Array* va, osg::Vec3Array* na, const osg::Vec4& color,
                                         osg::PrimitiveSet* p, bool autoNormals = true, bool useVBO = true);

    /** Create a polar sphere (r1 = r2 = r3) or ellipsoid */
    extern osg::Geometry* createEllipsoid(const osg::Vec3& center, float radius1, float radius2,
                                          float radius3, int samples = 32);

    /** Create a superellipsoid (see http://paulbourke.net/geometry/spherical/) */
    extern osg::Geometry* createSuperEllipsoid(const osg::Vec3& center, float radius, float power1,
                                               float power2, int samples = 32);

    /** Create a prism (n > 3) or cylinder (n is large enough) */
    extern osg::Geometry* createPrism(const osg::Vec3& centerBottom, float radiusBottom, float radiusTop,
                                      float height, int n = 4, bool capped = true);

    /** Create a pyramid (n > 3) or cone (n is large enough) */
    extern osg::Geometry* createPyramid(const osg::Vec3& centerBottom, float radius, float height,
                                        int n = 4, bool capped = false);

    /** Create a view frustum geometry corresponding to given matrices */
    extern osg::Geometry* createViewFrustumGeometry(const osg::Matrix& view, const osg::Matrix& proj);

    /** Create a geodesic sphere which has well-distributed facets */
    extern osg::Geometry* createGeodesicSphere(const osg::Vec3& center, float radius, int iterations = 4);

    /** Create a soccer-like geometry named truncated icosahedron */
    extern osg::Geometry* createSoccer(const osg::Vec3& center, float radius);

    /** Create a textured icosahedron for panorama use */
    extern osg::Geometry* createPanoramaSphere(int subdivs = 2);

    /** Create a bounding volume geometry */
    extern osg::Geometry* createBoundingBoxGeometry(const osg::BoundingBox& bb);
    extern osg::Geometry* createBoundingSphereGeometry(const osg::BoundingSphere& bs);
}

#endif
