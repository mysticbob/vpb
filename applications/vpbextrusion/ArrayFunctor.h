#ifndef ARRAYFUNCTOR_H_
#define ARRAYFUNCTOR_H_


#include <osg/Array>

// ** template ArrayVisitor to handle all method in one template.
// **  Only use when process done on each array could be templated

template <class T>
class ArrayFunctor : public osg::ArrayVisitor, public T
{
    public:
    	
    	virtual void apply(osg::Array&) {}
//        virtual void apply(osg::ByteArray& array) { T::process<osg::ByteArray>(array); }
//        virtual void apply(osg::ShortArray& array) { T::process(array); }
//        virtual void apply(osg::IntArray& array) { T::process<osg::IntArray>(array); }
//        virtual void apply(osg::UByteArray& array) { T::process<osg::UByteArray>(array); }
//        virtual void apply(osg::UShortArray& array) { T::process<osg::UShortArray>(array); }
//        virtual void apply(osg::UIntArray& array) { T::process<osg::UIntArray>(array); }
//        virtual void apply(osg::FloatArray& array) { T::process<osg::FloatArray>(array); }
//        virtual void apply(osg::DoubleArray& array) { T::process<osg::DoubleArray>(array); }

        virtual void apply(osg::Vec2Array & array) { T::process(array); }
        virtual void apply(osg::Vec3Array& array) { T::process(array); }
        virtual void apply(osg::Vec4Array& array) { T::process(array); }

        virtual void apply(osg::Vec4ubArray& array) { T::process(array); }

        virtual void apply(osg::Vec2bArray& array) { T::process(array); }
        virtual void apply(osg::Vec3bArray& array) { T::process(array); }
        virtual void apply(osg::Vec4bArray& array) { T::process(array); }

        virtual void apply(osg::Vec2sArray& array) { T::process(array); }
        virtual void apply(osg::Vec3sArray& array) { T::process(array); }
        virtual void apply(osg::Vec4sArray& array) { T::process(array); }

        virtual void apply(osg::Vec2dArray& array) { T::process(array); }
        virtual void apply(osg::Vec3dArray& array) { T::process(array); }
        virtual void apply(osg::Vec4dArray& array) { T::process(array); }
};

#endif /*ARRAYFUNCTOR_H_*/
