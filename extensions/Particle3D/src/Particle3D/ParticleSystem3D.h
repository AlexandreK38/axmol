/****************************************************************************
 Copyright (c) 2015-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

 https://axmol.dev/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#ifndef __AX_PARTICLE_SYSTEM_3D_H__
#define __AX_PARTICLE_SYSTEM_3D_H__

#include "2d/Node.h"
#include "math/Math.h"
#include <vector>
#include <map>
#include <list>
#include "extensions/ExtensionExport.h"

namespace ax
{

/**
 * 3d particle system
 */
class Particle3DEmitter;
class Particle3DAffector;
class Particle3DRender;

struct AX_EX_DLL Particle3D
{
    Particle3D();
    virtual ~Particle3D();
    // property of particles
    Vec3 position;           // position
    Quaternion orientation;  //  Orientation of the particle.
    Vec4 color;              // particle color
    Vec2 lb_uv;              // left bottom uv
    Vec2 rt_uv;              // right top uv
    float width;             // Own width
    float height;            // Own height
    float depth;             // Own depth

    // user defined property
    std::unordered_map<std::string, void*> userDefs;
};

template <typename T>
class AX_EX_DLL DataPool
{
public:
    typedef typename std::list<T*> PoolList;
    typedef typename std::list<T*>::iterator PoolIterator;

    DataPool(){};
    ~DataPool(){};

    T* createData()
    {
        if (_locked.empty())
            return nullptr;
        T* p = _locked.front();
        //_released.emplace_back(p);
        //_locked.erase(_locked.begin());
        _released.splice(_released.end(), _locked, _locked.begin());
        return p;
    }

    void lockLatestData()
    {
        _locked.emplace_back(*_releasedIter);
        _releasedIter = _released.erase(_releasedIter);
        if (_releasedIter != _released.begin() && _releasedIter != _released.end())
        {
            --_releasedIter;
        }
    }

    void lockData(T* data)
    {
        PoolIterator tempIter = _releasedIter;
        T* ptr                = getFirst();
        while (ptr)
        {
            if (ptr == data)
                break;
            ptr = getNext();
        }
        if (ptr)
            lockLatestData();
        _releasedIter = tempIter;
    }

    void lockAllDatas()
    {
        _locked.splice(_locked.end(), _released);
        //_locked.insert(_locked.end(), _released.begin(), _released.end());
        //_released.clear();
        _releasedIter = _released.begin();
    }

    T* getFirst()
    {
        _releasedIter = _released.begin();
        if (_releasedIter == _released.end())
            return nullptr;
        return *_releasedIter;
    }

    T* getNext()
    {
        if (_releasedIter == _released.end())
            return nullptr;
        ++_releasedIter;
        if (_releasedIter == _released.end())
            return nullptr;
        return *_releasedIter;
    }

    const PoolList& getActiveDataList() const { return _released; };
    const PoolList& getUnActiveDataList() const { return _locked; };

    void addData(T* data) { _locked.emplace_back(data); }

    bool empty() const { return _released.empty(); };

    void removeAllDatas()
    {
        lockAllDatas();
        for (auto&& iter : _locked)
        {
            delete iter;
        }
        _locked.clear();
    }

private:
    PoolIterator _releasedIter;
    PoolList _released;
    PoolList _locked;
};

typedef DataPool<Particle3D> ParticlePool;

class AX_EX_DLL ParticleSystem3D : public Node, public BlendProtocol
{
public:
    enum class State
    {
        STOP,
        RUNNING,
        PAUSE,
    };

    /**
     * override function
     */
    virtual void update(float delta) override;

    /**
     * override function
     */
    virtual void draw(Renderer* renderer, const Mat4& transform, uint32_t flags) override;

    /**
     * override function
     */
    virtual void setBlendFunc(const BlendFunc& blendFunc) override;

    /**
     * override function
     */
    virtual const BlendFunc& getBlendFunc() const override;

    /**
     * particle system play control
     */
    virtual void startParticleSystem();

    /**
     * stop particle
     */
    virtual void stopParticleSystem();

    /**
     * pause particle
     */
    virtual void pauseParticleSystem();

    /**
     * resume particle
     */
    virtual void resumeParticleSystem();

    /**
     * set emitter for particle system, can set your own particle emitter
     */
    void setEmitter(Particle3DEmitter* emitter);
    /**
     * set particle render, can set your own particle render
     */
    void setRender(Particle3DRender* render);
    /**
     * return particle render
     */
    Particle3DRender* getRender() { return _render; }
    /**
     * add particle affector
     */
    void addAffector(Particle3DAffector* affector);

    /**
     * remove affector by index
     */
    void removeAffector(int index);

    /**
     * remove all particle affector
     */
    void removeAllAffector();

    /**
     * get particle quota
     */
    unsigned int getParticleQuota() const;
    /**
     * set particle quota
     */
    void setParticleQuota(unsigned int quota);

    /**
     * get particle affector by index
     */
    Particle3DAffector* getAffector(int index);

    /**
     * get particle pool
     */
    const ParticlePool& getParticlePool() { return _particlePool; }

    /**
     * get alive particles count
     */
    virtual int getAliveParticleCount() const { return 0; }

    /**
     * get particle playing state
     */
    State getState() const { return _state; }

    bool isKeepLocal() const { return _keepLocal; }
    void setKeepLocal(bool keepLocal);

    /**
     *Enables or disables the system.
     */
    void setEnabled(bool enabled);

    /**
     * is enabled
     */
    bool isEnabled() const { return _isEnabled; }

    ParticleSystem3D();
    virtual ~ParticleSystem3D();

protected:
    State _state;
    Particle3DEmitter* _emitter;
    std::vector<Particle3DAffector*> _affectors;
    Particle3DRender* _render;

    // particles
    ParticlePool _particlePool;
    int _aliveParticlesCnt;
    unsigned int _particleQuota;

    BlendFunc _blend;

    bool _keepLocal;
    bool _isEnabled;
};

}

#endif
