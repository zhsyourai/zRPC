//
// Created by zhsyourai on 11/27/16.
//
#include <msgpack.hpp>
#include "zRPC/filter.h"
#include "zRPC/filter/msgpack_filter.h"
#include "zRPC/support/string_utils.h"
#include "zRPC/support/bytes_buf.h"
#include "zRPC/support/rtti.h"
#include "zRPC/rpc/call.h"

static inline void release_msgpack_object(msgpack::object &object) {
  switch (object.type) {
    case msgpack::type::NIL:break;
    case msgpack::type::BOOLEAN:break;
    case msgpack::type::POSITIVE_INTEGER:break;
    case msgpack::type::NEGATIVE_INTEGER:break;
    case msgpack::type::FLOAT32:break;
    case msgpack::type::FLOAT64:break;
    case msgpack::type::STR:delete[] object.via.str.ptr;
      break;
    case msgpack::type::BIN:delete[] object.via.bin.ptr;
      break;
    case msgpack::type::ARRAY:
      for (int i = 0; i < object.via.array.size; ++i) {
        release_msgpack_object(object.via.array.ptr[i]);
      }
      delete[] object.via.array.ptr;
      break;
    case msgpack::type::MAP:
      for (int i = 0; i < object.via.map.size; ++i) {
        release_msgpack_object(object.via.map.ptr[i].key);
        release_msgpack_object(object.via.map.ptr[i].val);
      }
      delete[] object.via.map.ptr;
      break;
    case msgpack::type::EXT:break;
  }
}

static inline msgpack::object zRPC_get_array(zRPC_value *value) {
  msgpack::object *array;
  msgpack::object ret;
  msgpack::object_array oa;
  array = new msgpack::object[value->array_value->len];
  for (int i = 0; i < value->array_value->len; ++i) {
    switch (value->array_value->value[i]->type) {
      case STR:array[i] = msgpack::object(value->array_value->value[i]->str_value.str);
        break;
      case INT64:array[i] = msgpack::object(value->array_value->value[i]->int64_value);
        break;
      case FLOAT:array[i] = msgpack::object(value->array_value->value[i]->float_value);
        break;
      case BYTE:
      default:array[i] = msgpack::object(value->array_value->value[i]->byte_value);
        break;
    }
  }
  oa.ptr = array;
  oa.size = (uint32_t) value->array_value->len;
  ret.via.array = oa;
  ret.type = msgpack::type::ARRAY;
  SUB_REFERENCE(value, zRPC_value);
  return ret;
};

static msgpack::object zRPC_base_value_to_msgpack_value(zRPC_value *value) {
  msgpack::object ret;
  switch (value->type) {
    case INT64:ret = msgpack::object(value->int64_value);
      SUB_REFERENCE(value, zRPC_value);
      break;
    case FLOAT:ret = msgpack::object(value->float_value);
      SUB_REFERENCE(value, zRPC_value);
      break;
    case STR:ret = msgpack::object(zRPC_str_dup(value->str_value.str));
      SUB_REFERENCE(value, zRPC_value);
      break;
    default:ret = msgpack::object(value->int64_value);
      SUB_REFERENCE(value, zRPC_value);
      break;
  }
  return ret;
}

static inline msgpack::object zRPC_get_map(zRPC_value *value) {
  msgpack::object ret;
  msgpack::object_kv *entities;
  msgpack::object_map om;
  entities = new msgpack::object_kv[value->map_value->len];
  for (int i = 0; i < value->map_value->len; ++i) {
    entities[i] = msgpack::object_kv();
    entities[i].key = zRPC_base_value_to_msgpack_value(PASS_PTR(value->map_value->value[i].key, zRPC_value));
    entities[i].val = zRPC_base_value_to_msgpack_value(PASS_PTR(value->map_value->value[i].value, zRPC_value));
  }
  om.ptr = entities;
  om.size = (uint32_t) value->map_value->len;
  ret.via.map = om;
  ret.type = msgpack::type::MAP;
  SUB_REFERENCE(value, zRPC_value);
  return ret;
};

static msgpack::object zRPC_value_to_msgpack_value(zRPC_value *value) {
  zRPC_value *value_pass;
  switch (value->type) {
    case ARRAY:value_pass = PASS_PTR(value, zRPC_value);
      SUB_REFERENCE(value, zRPC_value);
      return zRPC_get_array(value_pass);
    case MAP:value_pass = PASS_PTR(value, zRPC_value);
      SUB_REFERENCE(value, zRPC_value);
      return zRPC_get_map(value_pass);
    default:value_pass = PASS_PTR(value, zRPC_value);
      SUB_REFERENCE(value, zRPC_value);
      return zRPC_base_value_to_msgpack_value(value_pass);
  }
}

static inline zRPC_value *zRPC_get_zRPC_array(msgpack::object_array *array_value) {
  zRPC_value *ret_array;
  std::string str_v;
  int8_t byte_v;
  int64_t int64_v;
  float float_v;
  ret_array = zRPC_type_var_create_array(array_value->size);
  for (int i = 0; i < array_value->size; ++i) {
    switch (array_value->ptr[i].type) {
      case msgpack::type::NEGATIVE_INTEGER:
        int64_v = array_value->ptr[i].convert();
        ret_array->array_value->value[i] = zRPC_type_var_create_base(INT64, &int64_v);
        break;
      case msgpack::type::FLOAT:
      case msgpack::type::FLOAT32:
        float_v = array_value->ptr[i].convert();
        ret_array->array_value->value[i] = zRPC_type_var_create_base(FLOAT, &float_v);
        break;
      case msgpack::type::STR:
        array_value->ptr[i].convert(str_v);
        ret_array->array_value->value[i] = zRPC_type_var_create_base(STR, (void *) str_v.c_str());
        break;
      default:
        byte_v = array_value->ptr[i].convert();
        ret_array->array_value->value[i] = zRPC_type_var_create_base(BYTE, &byte_v);
        break;
    }
  }
  return ret_array;
};

static zRPC_value *msgpack_value_to_zRPC_base_value(msgpack::object value) {
  zRPC_value *ret_value;
  std::string str;
  float f_v;
  int64_t int_v;
  switch (value.type) {
    case msgpack::type::FLOAT:
    case msgpack::type::FLOAT32:
      f_v = value.convert();
      ret_value = zRPC_type_var_create_base(FLOAT, &f_v);
      break;
    case msgpack::type::STR:
      value.convert(str);
      ret_value = zRPC_type_var_create_base(STR, (void *) str.c_str());
      break;
    case msgpack::type::NEGATIVE_INTEGER:
    default:
      int_v = value.convert();
      ret_value = zRPC_type_var_create_base(INT64, &int_v);
      break;
  }
  return ret_value;
}

static inline zRPC_value *zRPC_get_zRPC_map(msgpack::object_map *map_value) {
  zRPC_value *ret_value = zRPC_type_var_create_map(map_value->size);
  for (int i = 0; i < map_value->size; ++i) {
    ret_value->map_value->value[i].key = msgpack_value_to_zRPC_base_value(map_value->ptr[i].key);
    ret_value->map_value->value[i].value = msgpack_value_to_zRPC_base_value(map_value->ptr[i].val);
  }
  return ret_value;
};

static zRPC_value *msgpack_value_to_zRPC_value(msgpack::object value) {
  zRPC_value *ret_value;
  switch (value.type) {
    case msgpack::type::ARRAY:
      ret_value = zRPC_get_zRPC_array(&value.via.array);
      break;
    case msgpack::type::MAP:
      ret_value = zRPC_get_zRPC_map(&value.via.map);
      break;
    default:
      ret_value = msgpack_value_to_zRPC_base_value(value);
      break;
  }
  return ret_value;
}

static void package(void *in, zRPC_bytes_buf **buf) {
  IF_TYPE_SAME(zRPC_call, in) {
    zRPC_call *call = (zRPC_call *) in;

    // serializes multiple objects using msgpack::packer.
    msgpack::sbuffer buffer;

    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    /* Init flag magic */
    pk.pack(std::string("ca"));

    /* Init function name */
    const char *name;
    zRPC_call_get_function(call, &name);
    pk.pack(std::string(name));

    /* Init request id*/
    pk.pack(call->request_id);

    /* Init function params */
    zRPC_call_param *params;
    unsigned int params_count;
    zRPC_call_get_params(call, &params, &params_count);
    pk.pack_map(params_count);
    for (int i = 0; i < params_count; ++i) {
      pk.pack(std::string(params[i].name));
      msgpack::object val = zRPC_value_to_msgpack_value(PASS_PTR(params[i].value, zRPC_value));
      pk.pack(val);
      release_msgpack_object(val);
    }

    zRPC_bytes_buf *ret_buf;
    zRPC_bytes_buf_create(buffer.size(), &ret_buf);

    memcpy(zRPC_bytes_buf_addr(ret_buf), buffer.data(), buffer.size());

    *buf = ret_buf;
  } ELSE_IF_TYPE_SAME(zRPC_call_result, in) {
    zRPC_call_result *result = (zRPC_call_result *) in;
    // serializes multiple objects using msgpack::packer.
    msgpack::sbuffer buffer;

    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    /* Init flag magic */
    pk.pack(std::string("cr"));

    /* Init request id*/
    pk.pack(result->request_id);

    /* Init results */
    zRPC_call_param *results;
    unsigned int result_count;
    zRPC_call_result_get_results(result, &results, &result_count);
    pk.pack_map(result_count);
    for (int i = 0; i < result_count; ++i) {
      pk.pack(std::string(results[i].name));
      msgpack::object val = zRPC_value_to_msgpack_value(PASS_PTR(results[i].value, zRPC_value));
      pk.pack(val);
      release_msgpack_object(val);
    }

    zRPC_bytes_buf *ret_buf;
    zRPC_bytes_buf_create((unsigned int) buffer.size(), &ret_buf);

    memcpy(zRPC_bytes_buf_addr(ret_buf), buffer.data(), buffer.size());

    *buf = ret_buf;
  }
}

static void unpackage(zRPC_bytes_buf *buf, void **out) {
  // deserializes these objects using msgpack::unpacker.
  msgpack::unpacker pac;

  // feeds the buffer.
  pac.reserve_buffer(zRPC_bytes_buf_len(buf));
  memcpy(pac.buffer(), zRPC_bytes_buf_addr(buf), zRPC_bytes_buf_len(buf));
  pac.buffer_consumed(zRPC_bytes_buf_len(buf));

  // now starts streaming deserialization.
  msgpack::object_handle oh;
  pac.next(oh);
  std::string flag = oh.get().convert();
  if ("ca" == flag) {
    zRPC_call *ret_call;
    zRPC_call_create(&ret_call);
    if (!pac.next(oh)) goto error;
    std::string name;
    oh.get().convert(name);
    zRPC_call_set_function(ret_call, name.c_str());

    if (!pac.next(oh)) goto error;
    int request_id;
    request_id = oh.get().convert();
    ret_call->request_id = request_id;

    if (!pac.next(oh)) goto error;
    const msgpack::object_map &map = oh.get().via.map;

    for (int i = 0; i < map.size; ++i) {
      std::string key;
      map.ptr[i].key.convert(key);
      zRPC_value *val = msgpack_value_to_zRPC_value(map.ptr[i].val);
      zRPC_call_set_param(ret_call, key.c_str(), PASS_PTR(val, zRPC_value));
    }
    *out = ret_call;
  } else if ("cr" == flag) {
    zRPC_call_result *call_result;
    zRPC_call_result_create(&call_result);
    if (!pac.next(oh)) goto error;
    int request_id;
    request_id = oh.get().convert();

    if (!pac.next(oh)) goto error;
    const msgpack::object_map &map = oh.get().via.map;

    call_result->request_id = request_id;

    for (int i = 0; i < map.size; ++i) {
      std::string key;
      map.ptr[i].key.convert(key);
      zRPC_value *val = msgpack_value_to_zRPC_value(map.ptr[i].val);
      zRPC_call_result_set_result(call_result, key.c_str(), PASS_PTR(val, zRPC_value));
    }
    *out = call_result;
  }
  return;
  error:
  *out = NULL;
}

void msgpack_filter_on_active(zRPC_filter *filter, zRPC_channel *channel) {

}

void
msgpack_filter_on_readable(zRPC_filter *filter, zRPC_channel *channel, void *msg, zRPC_filter_out *out) {
  zRPC_bytes_buf *buf = (zRPC_bytes_buf *) msg;
  zRPC_call *call;
  unpackage(buf, (void **) &call);
  zRPC_filter_out_add_item(out, call);
  SUB_REFERENCE(buf, zRPC_bytes_buf);
}

void
msgpack_filter_on_writable(zRPC_filter *filter, zRPC_channel *channel, void *msg, zRPC_filter_out *out) {
  zRPC_bytes_buf *buf;
  package(msg, &buf);
  zRPC_filter_out_add_item(out, PASS_PTR(buf, zRPC_bytes_buf));
  SUB_REFERENCE(buf, zRPC_bytes_buf);
}

void msgpack_filter_on_inactive(zRPC_filter *filter, zRPC_channel *channel) {
}

static zRPC_filter *msgpack_filter_create(void *factory_custom) {
  zRPC_filter *filter;
  zRPC_filter_create(&filter, NULL);

  zRPC_filter_set_on_active_callback(filter, msgpack_filter_on_active);
  zRPC_filter_set_on_read_callback(filter, msgpack_filter_on_readable);
  zRPC_filter_set_on_write_callback(filter, msgpack_filter_on_writable);
  zRPC_filter_set_on_inactive_callback(filter, msgpack_filter_on_inactive);
  return filter;
}

zRPC_filter_factory *msgpack_filter_factory_instance = NULL;

zRPC_filter_factory *msgpack_filter_factory() {
  if (msgpack_filter_factory_instance == NULL) {
    msgpack_filter_factory_instance = zRPC_filter_factory_create(msgpack_filter_create, NULL);
  }
  return msgpack_filter_factory_instance;
}

