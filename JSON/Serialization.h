#pragma once

#include <QJsonObject>
#include <QJsonArray>

#define HX_JOIN(...) __VA_ARGS__

#define HX_STR_INTERNAL(s) #s
#define HX_STR(s) HX_STR_INTERNAL(s)

#define HX_JSON_SCOPE(_jsonObject, ...) \
do { \
    auto& _json = _jsonObject; \
    HX_JOIN(__VA_ARGS__) \
} while(0);

#define HX_SERIALIZE(_object, _getter) _json[HX_STR(_object)] = _object->_getter()

#define HX_DESERIALIZE(_object, _setter, _jsonGetter) _object->_setter(_json[HX_STR(_object)]._jsonGetter())
