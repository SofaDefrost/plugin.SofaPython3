
#include <sofa/core/objectmodel/BaseNode.h>
#include <sofa/core/objectmodel/BaseData.h>
#include <sofa/defaulttype/DataTypeInfo.h>

#include <SofaPython3/DataHelper.h>
#include <SofaPython3/DataCache.h>

namespace sofapython3
{

std::string toSofaParsableString(const py::handle& p)
{
    if(py::isinstance<py::list>(p) || py::isinstance<py::tuple>(p))
    {
        std::stringstream tmp;
        for(auto pa : p){
            tmp << toSofaParsableString(pa) << " ";
        }
        return tmp.str();
    }
    //TODO(dmarchal) This conversion to string is so bad.
    if(py::isinstance<py::str>(p))
        return py::str(p);
    return py::repr(p);
}

/// RVO optimized function. Don't care about copy on the return code.
void fillBaseObjectdescription(sofa::core::objectmodel::BaseObjectDescription& desc,
                               const py::dict& dict)
{
    for(auto kv : dict)
    {
        desc.setAttribute(py::str(kv.first), toSofaParsableString(kv.second));
    }
}

BindingDataFactory* getBindingDataFactoryInstance(){
    static BindingDataFactory* s_localfactory = new BindingDataFactory();
    if (s_localfactory == nullptr)
    {
        s_localfactory = new BindingDataFactory();
    }
    return s_localfactory ;
}

PythonTrampoline::~PythonTrampoline(){}
void PythonTrampoline::setInstance(py::object s)
{
    py::print(py::str( s.get_type() ));

    s.inc_ref();

    // TODO(bruno-marques) ici ça crash dans SOFA.
    //--ref_counter;

    pyobject = std::shared_ptr<PyObject>( s.ptr(), [](PyObject* ob)
    {
            // runSofa Sofa/tests/pyfiles/ScriptController.py => CRASH
            // Py_DECREF(ob);
    });
}

std::ostream& operator<<(std::ostream& out, const py::buffer_info& p)
{
    out << "buffer{"<< p.format << ", " << p.ndim << ", " << p.shape[0];
    if(p.ndim==2)
        out << ", " << p.shape[1];
    out << ", " << p.size << "}";
    return out;
}

std::string getPathTo(Base* b)
{
    BaseNode* node = dynamic_cast<BaseNode*>(b);
    if(node)
        return node->getPathName();
    BaseObject* object = dynamic_cast<BaseObject*>(b);
    if(object)
        return object->getPathName();

    assert(true && "Only Base & BaseObject are supported");
    return "";
}

const char* getFormat(const AbstractTypeInfo& nfo)
{
    if(nfo.Integer())
    {
        return py::format_descriptor<long>::value;
    } else if(nfo.Scalar() )
    {
        if(nfo.byteSize() == 8)
            return py::format_descriptor<double>::value;
        else if(nfo.byteSize() == 4)
            return py::format_descriptor<float>::value;
    }
    return nullptr;
}

template<class Array, typename Value>
void setValueArray1D(Array p,
                     const py::slice& slice,
                     const Value& v)
{
    size_t start, stop, step, slicelength;
    if (!slice.compute(p.shape(0), &start, &stop, &step, &slicelength))
        throw py::error_already_set();

    for (size_t i = 0; i < slicelength; ++i) {
        p(start) = v;
        start += step;
    }
}

template<class Array, typename Value>
void setValueArray2D(Array p,
                     const py::slice& slice,
                     const Value& v)
{
    size_t start, stop, step, slicelength;
    if (!slice.compute(p.shape(0), &start, &stop, &step, &slicelength))
        throw py::error_already_set();

    for (size_t i = 0; i < slicelength; ++i, start+=step) {
        for(size_t j=0; j<p.shape(1);++j){
            p(start, j) = v;
        }
    }
}

template<class Array, typename Value>
void setItem2DTyped(Array a, py::slice slice, Value dvalue)
{
    size_t start, stop, step, slicelength;
    if (!slice.compute(a.shape(0), &start, &stop, &step, &slicelength))
        throw py::error_already_set();

    for(size_t i=0;i<slicelength;++i, start+=step)
        for(int j=0;j<a.shape(1);++j)
            a(start, j) = dvalue;
}

template<class Array, typename Value>
void setItem2DTyped(Array a, py::slice sliceI, py::slice sliceJ, Value dvalue)
{
    size_t startI, stopI, stepI, slicelengthI;
    if (!sliceI.compute(a.shape(0), &startI, &stopI, &stepI, &slicelengthI))
        throw py::error_already_set();

    size_t startJ, stopJ, stepJ, slicelengthJ;
    if (!sliceJ.compute(a.shape(1), &startJ, &stopJ, &stepJ, &slicelengthJ))
        throw py::error_already_set();

    for(size_t i=0;i<slicelengthI;++i, startI+=stepI)
    {
        for(size_t j=0, itJ=startJ;j<slicelengthJ;++j, itJ+=stepJ)
        {
            a(startI, itJ) = dvalue;
        }
    }
}

void setItem2D(py::array a, py::slice slice, py::object o)
{
    if(a.request().format=="d")
        setItem2DTyped(a.mutable_unchecked<double, 2>(), slice, py::cast<double>(o));
    else if(a.request().format=="f")
        setItem2DTyped(a.mutable_unchecked<float, 2>(), slice, py::cast<float>(o));
    else
        throw py::type_error("Invalid type");
}

void setItem2D(py::array a, const py::slice& slice, const py::slice& slice1, py::object o)
{
    if(a.request().format=="d")
        setItem2DTyped(a.mutable_unchecked<double, 2>(), slice, slice1, py::cast<double>(o));
    else if(a.request().format=="f")
        setItem2DTyped(a.mutable_unchecked<float, 2>(), slice, slice1, py::cast<float>(o));
    else
        throw py::type_error("Invalid type");
}

template<class Array, typename Value>
void setItem1DTyped(Array a, py::slice slice, Value dvalue)
{
    size_t start, stop, step, slicelength;
    if (!slice.compute(a.shape(0), &start, &stop, &step, &slicelength))
        throw py::error_already_set();

    for(size_t i=0;i<slicelength;++i, start+=step)
        a(start) = dvalue;
}

void setItem1D(py::array a, py::slice slice, py::object o)
{
    if(a.request().format=="d")
        setItem1DTyped(a.mutable_unchecked<double, 1>(), slice, py::cast<double>(o));
    else if(a.request().format=="f")
        setItem1DTyped(a.mutable_unchecked<float, 1>(), slice, py::cast<float>(o));
    else
        throw py::type_error("Invalid type");
}

void setItem(py::array a, py::slice slice, py::object value)
{
    if(a.ndim()>2)
        throw py::index_error("DataContainer can only operate on 1 or 2D array.");

    else if(a.ndim()==1)
        setItem1D(a, slice, value);

    else if(a.ndim()==2)
        setItem2D(a, slice, value);
}

py::slice toSlice(const py::object& o)
{
    if( py::isinstance<py::slice>(o))
        return py::cast<py::slice>(o);

    size_t v = py::cast<size_t>(o);
    return py::slice(v,v+1,1);
}


std::map<void*, py::array>& getObjectCache()
{
    static std::map<void*, py::array>* s_objectcache {nullptr} ;
    if(!s_objectcache)
    {
        s_objectcache=new std::map<void*, py::array>();
    }
    return *s_objectcache;
}

void trimCache()
{
    auto& memcache = getObjectCache();
    if(memcache.size() > 1000)
    {
        std::cout << "flushing the cache (it is too late to implement LRU)" << std::endl ;
        memcache.clear();
    }
}

py::buffer_info toBufferInfo(BaseData& m)
{
    const AbstractTypeInfo& nfo { *m.getValueTypeInfo() };
    auto itemNfo = nfo.BaseType();

    const char* format = nullptr;
    if(nfo.Integer())
    {
        if(nfo.byteSize() == 8)
            format = py::format_descriptor<unsigned long long>::value;
        else if(nfo.byteSize() == 4)
            format = py::format_descriptor<unsigned long>::value;
        else if(nfo.byteSize() == 2)
            format = py::format_descriptor<unsigned short>::value;
        else if(nfo.byteSize() == 1)
            format = py::format_descriptor<unsigned char>::value;
    } else if(nfo.Scalar() )
    {
        if(nfo.byteSize() == 8)
            format = py::format_descriptor<double>::value;
        else if(nfo.byteSize() == 4)
            format = py::format_descriptor<float>::value;
    }

    int rows = nfo.size(m.getValueVoidPtr()) / itemNfo->size();
    int cols = nfo.size();
    size_t datasize = nfo.byteSize();

    void* ptr = const_cast<void*>(nfo.getValuePtr(m.getValueVoidPtr()));
    if( !itemNfo->Container() ){
        return py::buffer_info(
                    ptr, /* Pointer to buffer */
                    datasize,                              /* Size of one scalar */
                    format,                                /* Python struct-style format descriptor */
                    1,                                     /* Number of dimensions */
        { rows },                              /* Buffer dimensions */
        { datasize }                           /* Strides (in bytes) for each index */

                    );
    }
    py::buffer_info ninfo(
                ptr,  /* Pointer to buffer */
                datasize,                              /* Size of one scalar */
                format,                                /* Python struct-style format descriptor */
                2,                                     /* Number of dimensions */
    { rows, cols },                        /* Buffer dimensions */
    { datasize * cols ,    datasize }                         /* Strides (in bytes) for each index */

                );
    return ninfo;
}

py::object convertToPython(BaseData* d)
{
    const AbstractTypeInfo& nfo{ *(d->getValueTypeInfo()) };
    if(hasArrayFor(d))
        return getPythonArrayFor(d);

    if(nfo.Container())
    {
        size_t dim0 = nfo.size(d->getValueVoidPtr())/nfo.size();
        size_t dim1 = nfo.size();
        py::list list;
        for(size_t i=0;i<dim0;i++)
        {
            py::list list1;
            for(size_t j=0;j<dim1;j++)
            {
                if(nfo.Integer())
                    list1.append(nfo.getIntegerValue(d->getValueVoidPtr(),i*dim1+j));
                if(nfo.Scalar())
                    list1.append(nfo.getScalarValue(d->getValueVoidPtr(),i*dim1+j));
                if(nfo.Text())
                    list1.append(nfo.getTextValue(d->getValueVoidPtr(),0));
            }
            list.append(list1);
        }
        return std::move(list);
    }

    if(nfo.Integer())
        return py::cast(nfo.getIntegerValue(d->getValueVoidPtr(), 0));
    if(nfo.Text())
        return py::cast(d->getValueString());
    if(nfo.Scalar())
        return py::cast(nfo.getScalarValue(d->getValueVoidPtr(), 0));

    std::cout << nfo.name() << " is not a container nor a scalar." << std::endl;

    if (!getBindingDataFactoryInstance()->createObject(nfo.name(), d).is_none())
        return getBindingDataFactoryInstance()->createObject(nfo.name(), d);

    std::cout << nfo.name() << " No such type in the BindingDataFactory. returning string" << std::endl;

    return py::cast(d->getValueString());
}

bool hasArrayFor(BaseData* d)
{
    auto& memcache = getObjectCache();
    return memcache.find(d) != memcache.end();
}

py::array resetArrayFor(BaseData* d)
{
    //todo: protect the function.
    auto& memcache = getObjectCache();
    auto capsule = py::capsule(new Base::SPtr(d->getOwner()));

    py::buffer_info ninfo = toBufferInfo(*d);
    py::array a(pybind11::dtype(ninfo), ninfo.shape,
                ninfo.strides, ninfo.ptr, capsule);

    memcache[d] = a;
    return a;
}

py::array getPythonArrayFor(BaseData* d)
{
    auto& memcache = getObjectCache();
    if(memcache.find(d) == memcache.end())
    {
        auto capsule = py::capsule(new Base::SPtr(d->getOwner()));

        py::buffer_info ninfo = toBufferInfo(*d);
        py::array a(pybind11::dtype(ninfo), ninfo.shape,
                    ninfo.strides, ninfo.ptr, capsule);

        memcache[d] = a;
        return a;
    }
    return memcache[d];
}

/// Make a python version of our BaseData.
/// Downcast everything.
py::object dataToPython(BaseData* d)
{
    const AbstractTypeInfo& nfo{ *(d->getValueTypeInfo()) };
    /// In case the data is a container with a simple layout
    /// we can expose the field as a numpy.array (no copy)
    if(nfo.Container() && nfo.SimpleLayout())
    {
        return getBindingDataFactoryInstance()->createObject("DataContainer", d);
    }

    return py::cast(d);
}

/// Make a python version of our BaseData.
/// If possible the data is exposed as a numpy.array to minmize copy and data conversion
py::object toPython(BaseData* d, bool writeable)
{

    const AbstractTypeInfo& nfo{ *(d->getValueTypeInfo()) };
    /// In case the data is a container with a simple layout
    /// we can expose the field as a numpy.array (no copy)
    if(nfo.Container() && nfo.SimpleLayout())
    {
        if(writeable)
            return getPythonArrayFor(d);
    }
    /// If this is not the case we return the converted datas (copy)
    return convertToPython(d);
}

// const implementation. always returning readonly objects
py::object toPython(const BaseData* d)
{
    return convertToPython(const_cast<BaseData*>(d));
}


void copyFromListScalar(BaseData& d, const AbstractTypeInfo& nfo, const py::list& l)
{
    /// Check if the data is a single dimmension or not.
    py::buffer_info dstinfo = toBufferInfo(d);

    if(dstinfo.ndim>2)
        throw py::index_error("Invalid number of dimension only 1 or 2 dimensions are supported).");

    if(dstinfo.ndim==1)
    {
        void* ptr = d.beginEditVoidPtr();

        if( size_t(dstinfo.shape[0]) != l.size())
            nfo.setSize(ptr, l.size());
        if (nfo.Scalar())
            for(size_t i = 0; i < l.size(); ++i)
            {
                nfo.setScalarValue(ptr, i, py::cast<double>(l[i]));
            }
        if (nfo.Integer())
            for(size_t i = 0; i < l.size(); ++i)
            {
                nfo.setIntegerValue(ptr, i, py::cast<long long>(l[i]));
            }

        d.endEditVoidPtr();
        return;
    }
    void* ptr = d.beginEditVoidPtr();
    if( size_t(dstinfo.shape[0]) != l.size() && !nfo.FixedSize())
        nfo.setSize(ptr, l.size());

    for(size_t i = 0; i < l.size(); ++i)
    {
        py::list ll = l[i];
        for(size_t j = 0; j < ll.size(); ++j)
        {
            nfo.setScalarValue(ptr, i*ll.size()+j, py::cast<double>(ll[j]));
        }
    }
    d.endEditVoidPtr();
    return;
}

void fromPython(BaseData* d, const py::object& o)
{
    const AbstractTypeInfo& nfo{ *(d->getValueTypeInfo()) };
    if(!nfo.Container())
    {
        scoped_writeonly_access guard(d);
        if(nfo.Integer())
            nfo.setIntegerValue(guard.ptr, 0, py::cast<int>(o));
        if(nfo.Text())
            nfo.setTextValue(guard.ptr, 0, py::cast<py::str>(o));
        if(nfo.Scalar())
            nfo.setScalarValue(guard.ptr, 0, py::cast<double>(o));
        return ;
    }

    if(nfo.Scalar() || nfo.Integer())
        return copyFromListScalar(*d, nfo, o);

    msg_error("SofaPython3") << "binding problem, trying to set value for "
                             << d->getName() << ", " << py::cast<std::string>(py::str(o));
}


}
