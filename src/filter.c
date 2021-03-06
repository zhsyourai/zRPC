//
// Created by zhswo on 2016/11/23.
//

#include <malloc.h>
#include <memory.h>
#include "zRPC/filter.h"


struct zRPC_filter {
    filter_callback on_active;
    filter_callback on_inactive;
    filter_callback_data on_read;
    filter_callback_data on_write;
    void *custom_data;
};

struct zRPC_filter_factory {
    void *custom_data;
    filter_factory_create create;
};

struct zRPC_filter_out {
    void **items;
    int count;
    int cap;
};

zRPC_filter_factory *zRPC_filter_factory_create(filter_factory_create factory, void *custom) {
    zRPC_filter_factory *ret = malloc(sizeof(zRPC_filter_factory));
    ret->custom_data = custom;
    ret->create = factory;
    return ret;
}

void zRPC_filter_create_by_factory(zRPC_filter **out, zRPC_filter_factory *factory) {
    *out = factory->create(factory->custom_data);
}

void zRPC_filter_create(zRPC_filter **out, void *custom_data) {
    zRPC_filter *filter = malloc(sizeof(zRPC_filter));
    filter->custom_data = custom_data;
    *out = filter;
}

void zRPC_filter_destroy(zRPC_filter *filter) {
    if (filter) {
        if (filter->custom_data)
            free(filter->custom_data);
        free(filter);
    }
}

void zRPC_filter_set_on_active_callback(zRPC_filter *filter, filter_callback callback) {
    filter->on_active = callback;
}

void zRPC_filter_set_on_inactive_callback(zRPC_filter *filter, filter_callback callback) {
    filter->on_inactive = callback;
}

void zRPC_filter_set_on_read_callback(zRPC_filter *filter, filter_callback_data callback) {
    filter->on_read = callback;
}

void zRPC_filter_set_on_write_callback(zRPC_filter *filter, filter_callback_data callback) {
    filter->on_write = callback;
}

void zRPC_filter_set_custom_data(zRPC_filter *filter, void *custom_data) {
    filter->custom_data = custom_data;
}

void *zRPC_filter_get_custom_data(zRPC_filter *filter) {
    return filter->custom_data;
}

void zRPC_filter_call_on_active(zRPC_filter *filter, struct zRPC_channel *channel) {
    (*filter->on_active)(filter, channel);
}


void zRPC_filter_call_on_inactive(zRPC_filter *filter, struct zRPC_channel *channel) {
    (*filter->on_inactive)(filter, channel);
}

void zRPC_filter_call_on_read(zRPC_filter *filter, struct zRPC_channel *channel, void *msg, zRPC_filter_out *out) {
    (*filter->on_read)(filter, channel, msg, out);
}

void zRPC_filter_call_on_write(zRPC_filter *filter, struct zRPC_channel *channel, void *msg, zRPC_filter_out *out) {
    (*filter->on_write)(filter, channel, msg, out);
}

void zRPC_filter_out_create(zRPC_filter_out **out) {
    zRPC_filter_out *filter_out = malloc(sizeof(zRPC_filter_out));
    filter_out->cap = 10;
    filter_out->count = 0;
    filter_out->items = malloc(sizeof(*filter_out->items) * filter_out->cap);
    bzero(filter_out->items, sizeof(*filter_out->items) * filter_out->cap);
    *out = filter_out;
}

void zRPC_filter_out_destroy(zRPC_filter_out *out) {
    if (out != NULL) {
        if (out->items != NULL) {
            free(out->items);
        }
        free(out);
    }
}

int zRPC_filter_out_item_count(zRPC_filter_out *out) {
    return out->count;
}

void zRPC_filter_out_add_item(zRPC_filter_out *out, void *item) {
    if (out->count >= out->cap) {
        out->cap *= 2;
        out->items = realloc(out->items, sizeof(*out->items) * out->cap);
    }
    out->items[out->count++] = item;
}

void zRPC_filter_out_remove_item(zRPC_filter_out *out, void *item) {
    for (int i = 0; i < out->count; ++i) {
        if (out->items[i] == item) {
            out->items[i] = NULL;
            break;
        }
    }
}

void *zRPC_filter_out_get_item(zRPC_filter_out *out, unsigned int i) {
    if (i >= out->count) {
        return NULL;
    }
    return out->items[i];
}
