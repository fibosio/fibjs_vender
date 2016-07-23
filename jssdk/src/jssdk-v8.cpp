/*
 *  jssdk-v8.cpp
 *  Created on: Mar 31, 2016
 *
 *  Copyright (c) 2016 by Leo Hoo
 *  lion@9465.net
 */

#include "jssdk-v8.h"
#include "libplatform/libplatform.h"
#include <stdlib.h>
#include <string.h>

namespace js
{

class v8_Runtime : public Runtime
{
private:
	class ShellArrayBufferAllocator : public v8::ArrayBuffer::Allocator
	{
	public:
		virtual void* Allocate(size_t length)
		{
			void* data = AllocateUninitialized(length);
			return data == NULL ? data : memset(data, 0, length);
		}

		virtual void* AllocateUninitialized(size_t length)
		{
			return malloc(length);
		}

		virtual void Free(void* data, size_t)
		{
			free(data);
		}
	};

public:
	v8_Runtime()
	{
		create_params.array_buffer_allocator = &array_buffer_allocator;
		m_isolate = v8::Isolate::New(create_params);

		v8::Locker locker(m_isolate);
		v8::HandleScope handle_scope(m_isolate);
		v8::Isolate::Scope isolate_scope(m_isolate);

		m_context.Reset(m_isolate, v8::Context::New(m_isolate));

		// v8::Number::New(m_isolate, 100);
	}

public:
	void destroy()
	{
		m_isolate->Dispose();
		delete this;
	}

	void Locker_enter(Locker& locker)
	{
		new (locker.m_locker) v8::Locker(m_isolate);
	}

	void Locker_leave(Locker& locker)
	{
		((v8::Locker*)locker.m_locker)->~Locker();
	}

	void Unlocker_enter(Unlocker& unlocker)
	{
		new (unlocker.m_unlocker) v8::Unlocker(m_isolate);
	}

	void Unlocker_leave(Unlocker& unlocker)
	{
		((v8::Unlocker*)unlocker.m_unlocker)->~Unlocker();
	}

	void Scope_enter(Scope& scope)
	{
		class HandleScope : public v8::HandleScope
		{
		public:
			HandleScope(v8::Isolate *rt) : v8::HandleScope(rt)
			{}

		public:
			static void *operator new(size_t, void * p)
			{
				return p;
			}
		};

		new (scope.m_locker) v8::Locker(m_isolate);
		new (scope.m_handle_scope) HandleScope(m_isolate);
		m_isolate->Enter();
		v8::Local<v8::Context>::New(m_isolate, m_context)->Enter();
	}

	void Scope_leave(Scope& scope)
	{
		v8::Local<v8::Context>::New(m_isolate, m_context)->Exit();
		m_isolate->Exit();
		((v8::HandleScope*)scope.m_handle_scope)->~HandleScope();
		((v8::Locker*)scope.m_locker)->~Locker();
	}

	Value execute(exlib::string code, exlib::string soname)
	{
		v8::Local<v8::Context> context = v8::Context::New(m_isolate);
		v8::Local<v8::String> str_code = v8::String::NewFromUtf8(m_isolate,
		                                 code.c_str(), v8::String::kNormalString,
		                                 (int32_t)code.length());
		v8::Local<v8::String> str_name = v8::String::NewFromUtf8(m_isolate,
		                                 soname.c_str(), v8::String::kNormalString,
		                                 (int32_t)soname.length());
		v8::Local<v8::Script> script = v8::Script::Compile(str_code, str_name);

		return Value(this, script->Run(context).ToLocalChecked());
	}

private:
	v8::Isolate *m_isolate;
	v8::Persistent<v8::Context> m_context;

	v8::Isolate::CreateParams create_params;
	ShellArrayBufferAllocator array_buffer_allocator;

	friend class Api_v8;
};

class Api_v8 : public Api
{
public:
	virtual const char* getEngine()
	{
		return "v8";
	}

	virtual int32_t getVersion()
	{
		return version;
	}

	virtual void init()
	{
		v8::Platform *platform = v8::platform::CreateDefaultPlatform();
		v8::V8::InitializePlatform(platform);

		v8::V8::Initialize();
	}

	virtual Runtime* createRuntime()
	{
		return new v8_Runtime();
	}

	bool ValueIsUndefined(const Value& v)
	{
		return !v.m_v.IsEmpty() && v.m_v->IsUndefined();
	}

	Value NewBoolean(Runtime* rt, bool b)
	{
		return Value(rt, b ? v8::True(((v8_Runtime*)rt)->m_isolate) :
		             v8::False(((v8_Runtime*)rt)->m_isolate));
	}

	bool ValueToBoolean(const Value& v)
	{
		return v.m_v->BooleanValue();
	}

	bool ValueIsBoolean(const Value& v)
	{
		return !v.m_v.IsEmpty() && (v.m_v->IsBoolean() || v.m_v->IsBooleanObject());
	}

	Value NewNumber(Runtime* rt, double d)
	{
		return Value(rt, v8::Number::New(((v8_Runtime*)rt)->m_isolate, d));
	}

	double ValueToNumber(const Value& v)
	{
		return v.m_v->NumberValue();
	}

	bool ValueIsNumber(const Value& v)
	{
		return !v.m_v.IsEmpty() && (v.m_v->IsNumber() || v.m_v->IsNumberObject());
	}

	Value NewString(Runtime* rt, exlib::string s)
	{
		return Value(rt, v8::String::NewFromUtf8(((v8_Runtime*)rt)->m_isolate,
		             s.c_str(), v8::String::kNormalString,
		             (int32_t)s.length()));
	}

	exlib::string ValueToString(const Value& v)
	{
		v8::String::Utf8Value tmp(v.m_v);
		if (*tmp)
			return exlib::string(*tmp, tmp.length());
		return exlib::string();
	}

	bool ValueIsString(const Value& v)
	{
		return !v.m_v.IsEmpty() && (v.m_v->IsString() || v.m_v->IsStringObject());
	}

	Object NewObject(Runtime* rt)
	{
		return Value(rt, v8::Object::New(((v8_Runtime*)rt)->m_isolate));
	}

	bool ObjectHas(const Object& o, exlib::string key)
	{
		return v8::Local<v8::Object>::Cast(o.m_v)->Has(
		           v8::String::NewFromUtf8(((v8_Runtime*)o.m_rt)->m_isolate,
		                                   key.c_str(), v8::String::kNormalString,
		                                   (int32_t)key.length()));
	}

	Value ObjectGet(const Object& o, exlib::string key)
	{
		return Value(o.m_rt, v8::Local<v8::Object>::Cast(o.m_v)->Get(
		                 v8::String::NewFromUtf8(((v8_Runtime*)o.m_rt)->m_isolate,
		                         key.c_str(), v8::String::kNormalString,
		                         (int32_t)key.length())));
	}

	void ObjectSet(const Object& o, exlib::string key, const Value& v)
	{
		v8::Local<v8::Object>::Cast(o.m_v)->Set(
		    v8::String::NewFromUtf8(((v8_Runtime*)o.m_rt)->m_isolate,
		                            key.c_str(), v8::String::kNormalString,
		                            (int32_t)key.length()), v.m_v);
	}

	void ObjectRemove(const Object& o, exlib::string key)
	{
		v8::Local<v8::Object>::Cast(o.m_v)->Delete(
		    v8::String::NewFromUtf8(((v8_Runtime*)o.m_rt)->m_isolate,
		                            key.c_str(), v8::String::kNormalString,
		                            (int32_t)key.length()));
	}

	Array ObjectKeys(const Object& o)
	{
		return Array(o.m_rt, v8::Local<v8::Object>::Cast(o.m_v)->GetPropertyNames());
	}

	bool ValueIsObject(const Value& v)
	{
		return !v.m_v.IsEmpty() && v.m_v->IsObject();
	}

	Array NewArray(Runtime* rt, int32_t sz)
	{
		return Value(rt, v8::Array::New(((v8_Runtime*)rt)->m_isolate, sz));
	}

	int32_t ArrayGetLength(const Array& a)
	{
		return v8::Local<v8::Array>::Cast(a.m_v)->Length();
	}

	Value ArrayGet(const Array& a, int32_t idx)
	{
		return Value(a.m_rt, v8::Local<v8::Array>::Cast(a.m_v)->Get(idx));
	}

	void ArraySet(const Array& a, int32_t idx, const Value& v)
	{
		v8::Local<v8::Array>::Cast(a.m_v)->Set(idx, v.m_v);
	}

	void ArrayRemove(const Array& a, int32_t idx)
	{
		v8::Local<v8::Array>::Cast(a.m_v)->Delete(idx);
	}

	bool ValueIsArray(const Value& v)
	{
		return !v.m_v.IsEmpty() && v.m_v->IsArray();
	}
};

static Api_v8 s_api;
Api* _v8_api = &s_api;

}