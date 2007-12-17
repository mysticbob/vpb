#ifndef EDGECOLLECTOR_H_
#define EDGECOLLECTOR_H_


#include <set>
#include <map>
#include <list>
#include <vector>
#include <algorithm>

#include <osg/Array>
#include <osg/Geometry>
#include <osg/ref_ptr>

struct dereference_less
{
    template<class T, class U>
    inline bool operator() (const T& lhs,const U& rhs) const
    {
        return *lhs < *rhs;
    }
};

template<class T>
bool dereference_check_less(const T& lhs,const T& rhs)
{
    if (lhs==rhs) return false;
    if (!lhs) return true;
    if (!rhs) return false;
    return *lhs < *rhs;
}

struct dereference_clear
{
    template<class T>
    inline void operator() (const T& t)
    {
        T& non_const_t = const_cast<T&>(t);
        non_const_t->clear();
    }
};

class EdgeCollector
{
public:

    struct Triangle;
    struct Edge;
    struct Edgeloop;
    struct Point;

    ~EdgeCollector();

    void setGeometry(osg::Geometry* geometry);
    osg::Geometry* getGeometry() { return _geometry; }

    unsigned int getNumOfTriangles() { return _triangleSet.size(); }

    typedef std::set<osg::ref_ptr<Edge>,dereference_less >                      EdgeSet;
    typedef std::vector<osg::ref_ptr<Edge> >                                    EdgeList;
    typedef std::list< osg::ref_ptr<Edgeloop> >                                 EdgeloopList;
    typedef std::set< osg::ref_ptr<Point>,dereference_less >                    PointSet;
    typedef std::vector< osg::ref_ptr<Point> >                                  PointList;
    typedef std::list< osg::ref_ptr<Triangle> >                                 TriangleList;
    typedef std::set< osg::ref_ptr<Triangle> >                                  TriangleSet;
    typedef std::map< osg::ref_ptr<Triangle>, unsigned int, dereference_less >  TriangleMap;

    struct Point : public osg::Referenced
    {
        Point(): _protected(false), _index(0) {}
        
        bool _protected;

        unsigned int _index;

        osg::Vec3           _vertex;
        TriangleSet         _triangles;

        void clear() { _triangles.clear(); }

        bool operator < ( const Point& rhs) const { return _vertex < rhs._vertex; }
        
        bool isBoundaryPoint() const;
    };

    struct Edge : public osg::Referenced
    {
        void clear();

        osg::ref_ptr<Point> _p1;
        osg::ref_ptr<Point> _p2;
        
        osg::ref_ptr<Point> _op1;
        osg::ref_ptr<Point> _op2;
        
        TriangleSet _triangles;

        
        bool operator < ( const Edge& rhs) const;
        
        bool operator == ( const Edge& rhs) const;

        bool operator != ( const Edge& rhs) const;
        
        void setOrderedPoints(Point* p1, Point* p2);
        
        void addTriangle(Triangle* triangle) { _triangles.insert(triangle); }
        
        bool isBoundaryEdge() const { return _triangles.size()<=1; }
        
        bool isAdjacentToBoundary() const { return isBoundaryEdge() || _p1->isBoundaryPoint() || _p2->isBoundaryPoint(); }
        
        bool endConnected(const Edge& rhs) const { return (_op2 == rhs._op1); }
        
        bool beginConnected(const Edge& rhs) const { return (_op1 == rhs._op2); }
    };

    struct Triangle : public osg::Referenced
    {
        Triangle() {}
        
        void clear();

        inline bool operator < (const Triangle& rhs) const;


        void setOrderedPoints(Point* p1, Point* p2, Point* p3);
        
        float distance(const osg::Vec3& vertex) const { return _plane.distance(vertex); }
        
        bool isBoundaryTriangle() const
        { return (_e1->isBoundaryEdge() || _e2->isBoundaryEdge() ||  _e3->isBoundaryEdge()); }

       
        osg::ref_ptr<Point> _p1;
        osg::ref_ptr<Point> _p2;
        osg::ref_ptr<Point> _p3;
        
        osg::ref_ptr<Point> _op1;
        osg::ref_ptr<Point> _op2;
        osg::ref_ptr<Point> _op3;
        
        osg::ref_ptr<Edge> _e1;
        osg::ref_ptr<Edge> _e2;
        osg::ref_ptr<Edge> _e3;
        
        osg::Plane _plane;        
    };

    class Edgeloop : public osg::Referenced
    {
        public:
            typedef std::vector<osg::ref_ptr<Edge> > EdgeList;
            
            bool isClosed() { return (_edgeList.back()->endConnected(*_edgeList.front().get())); }
            
            osg::UIntArray * toIndexArray() const;
            
            EdgeList _edgeList;
    };
    
    Triangle* addTriangle(unsigned int p1, unsigned int p2, unsigned int p3);
    Triangle* addTriangle(Point* p1, Point* p2, Point* p3);

    Edge* addEdge(Triangle* triangle, Point* p1, Point* p2);
    
    Point* addPoint(Triangle* triangle, unsigned int p1) { return addPoint(triangle,_originalPointList[p1].get()); }
    Point* addPoint(Triangle* triangle, Point* point);
    
    void getBoundaryEdgeList(EdgeList & el);
    bool extractBoundaryEdgeloop(EdgeList & el, Edgeloop & edgeloop);
    bool extractBoundaryEdgeloopList(EdgeList & el, EdgeloopList & edgeloopList);
    
//protected:
    
    osg::Geometry*                  _geometry;
    
    EdgeSet                         _edgeSet;
    TriangleSet                     _triangleSet;
    PointSet                        _pointSet;
    PointList                       _originalPointList;
    
};


#endif /*EDGECOLLECTOR_H_*/
