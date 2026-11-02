/*
* Copyright (c) 2020 ChangSha JingJiaMicro Electronics Co., Ltd.
* All rights reserved.
*
* Author:
*      shanjinkui <shanjinkui@jingjiamicro.com>
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice (including the next
* paragraph) shall be included in all copies or substantial portions of the
* Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* ChangSha JingJiaMicro Electronics Co., Ltd. AND/OR ITS SUPPLIERS BE LIABLE
* FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <assert.h>

#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"

#include "util/format/u_format.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/u_sampler.h"

#include "vl/vl_decoder.h"
#include "vl/vl_video_buffer.h"

#include "mwv207_log.h"
#include "mwv207_screen.h"
#include "mwv207_video_buffer.h"

static const unsigned const_resource_plane_order_YUV[3] = {0, 1, 2};

static const unsigned const_resource_plane_order_YVU[3] = {0, 2, 1};

void
mwv207_get_video_buffer_formats(struct pipe_screen *screen,
                               enum pipe_format format,
                               enum pipe_format out_format[VL_NUM_COMPONENTS])
{
   unsigned num_planes = util_format_get_num_planes(format);
   unsigned i;

   for (i = 0; i < num_planes; i++)
      out_format[i] = util_format_get_plane_format(format, i);
   for (; i < VL_NUM_COMPONENTS; i++)
      out_format[i] = PIPE_FORMAT_NONE;

   if (format == PIPE_FORMAT_YUYV)
      out_format[0] = PIPE_FORMAT_R8G8_R8B8_UNORM;
   else if (format == PIPE_FORMAT_UYVY)
      out_format[0] = PIPE_FORMAT_G8R8_B8R8_UNORM;
}

const unsigned *
mwv207_video_buffer_plane_order(enum pipe_format format)
{
   switch (format) {
   case PIPE_FORMAT_YV12:
      return const_resource_plane_order_YVU;

   case PIPE_FORMAT_NV12:
   case PIPE_FORMAT_R8G8B8A8_UNORM:
   case PIPE_FORMAT_B8G8R8A8_UNORM:
   case PIPE_FORMAT_YUYV:
   case PIPE_FORMAT_UYVY:
   case PIPE_FORMAT_P010:
   case PIPE_FORMAT_P016:
      return const_resource_plane_order_YUV;

   default:
      return NULL;
   }
}

static enum pipe_format
mwv207_video_buffer_surface_format(enum pipe_format format)
{
   const struct util_format_description *desc =
      util_format_description(format);

   if (desc->layout == UTIL_FORMAT_LAYOUT_SUBSAMPLED)
      return PIPE_FORMAT_R8G8B8A8_UNORM;

   return format;
}

bool
mwv207_video_buffer_is_format_supported(struct pipe_screen *screen,
                                       enum pipe_format format,
                                       enum pipe_video_profile profile,
                                       enum pipe_video_entrypoint entrypoint)
{
   enum pipe_format resource_formats[VL_NUM_COMPONENTS];
   unsigned i;

   mwv207_get_video_buffer_formats(screen, format, resource_formats);

   for (i = 0; i < VL_NUM_COMPONENTS; ++i) {
      enum pipe_format format = resource_formats[i];

      if (format == PIPE_FORMAT_NONE)
         continue;

      if (!screen->is_format_supported(
             screen, format, PIPE_TEXTURE_2D, 0, 0, PIPE_BIND_SAMPLER_VIEW))
         return false;

      format = mwv207_video_buffer_surface_format(format);
      if (!screen->is_format_supported(
             screen, format, PIPE_TEXTURE_2D, 0, 0, PIPE_BIND_RENDER_TARGET))
         return false;
   }

   return true;
}

unsigned
mwv207_video_buffer_max_size(struct pipe_screen *screen)
{
   return screen->get_param(screen, PIPE_CAP_MAX_TEXTURE_2D_SIZE);
}

void
mwv207_video_buffer_set_associated_data(
   struct pipe_video_buffer *vbuf,
   struct pipe_video_codec *vcodec,
   void *associated_data,
   void (*destroy_associated_data)(void *))
{
   vbuf->codec = vcodec;

   if (vbuf->associated_data == associated_data)
      return;

   if (vbuf->associated_data)
      vbuf->destroy_associated_data(vbuf->associated_data);

   vbuf->associated_data = associated_data;
   vbuf->destroy_associated_data = destroy_associated_data;
}

void *
mwv207_video_buffer_get_associated_data(struct pipe_video_buffer *vbuf,
                                       struct pipe_video_codec *vcodec)
{
   if (vbuf->codec == vcodec)
      return vbuf->associated_data;
   else
      return NULL;
}

void
mwv207_video_buffer_template(struct pipe_resource *templ,
                            const struct pipe_video_buffer *tmpl,
                            enum pipe_format resource_format,
                            unsigned depth,
                            unsigned array_size,
                            unsigned usage,
                            unsigned plane)
{
   unsigned height = tmpl->height;

   memset(templ, 0, sizeof(*templ));
   if (depth > 1)
      templ->target = PIPE_TEXTURE_3D;
   else if (array_size > 1)
      templ->target = PIPE_TEXTURE_2D_ARRAY;
   else
      templ->target = PIPE_TEXTURE_2D;
   templ->format = resource_format;
   templ->width0 = tmpl->width;
   templ->depth0 = depth;
   templ->array_size = array_size;
   templ->bind =
      PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET | tmpl->bind;
   templ->usage = usage;
   templ->flags = MWV207_RESOURCE_FLAG_VIDEO_BUFFER;

   // mwv207_video_buffer_adjust_size(
   //    &templ->width0, &height, plane, tmpl->codec->chroma_format, false);
   mwv207_video_buffer_adjust_size(
      &templ->width0, &height, plane, PIPE_VIDEO_CHROMA_FORMAT_420, false);      
   
   templ->height0 = height;
}

void
mwv207_video_buffer_destroy(struct pipe_video_buffer *buffer)
{
   struct mwv207_video_buffer *buf = (struct mwv207_video_buffer *)buffer;
   unsigned i;

   assert(buf);

   for (i = 0; i < VL_NUM_COMPONENTS; ++i) {
      pipe_sampler_view_reference(&buf->sampler_view_planes[i], NULL);
      pipe_sampler_view_reference(&buf->sampler_view_components[i], NULL);
      pipe_resource_reference(&buf->resources[i], NULL);
   }

   for (i = 0; i < VL_MAX_SURFACES; ++i)
      pipe_surface_reference(&buf->surfaces[i], NULL);

   mwv207_video_buffer_set_associated_data(buffer, NULL, NULL, NULL);

   if (buf->res) {
      pipe_resource_reference((struct pipe_resource **)&buf->res, NULL);

      buf->virtual_addr = NULL;
   }

   FREE(buf);
}

static struct pipe_sampler_view **
mwv207_video_buffer_sampler_view_planes(struct pipe_video_buffer *buffer)
{
   struct mwv207_video_buffer *buf = (struct mwv207_video_buffer *)buffer;
   struct pipe_sampler_view sv_templ;
   struct pipe_context *pipe;
   unsigned i;

   assert(buf);

   pipe = buf->base.context;

   for (i = 0; i < buf->num_planes; ++i) {
      if (!buf->sampler_view_planes[i]) {
         memset(&sv_templ, 0, sizeof(sv_templ));
         u_sampler_view_default_template(
            &sv_templ, buf->resources[i], buf->resources[i]->format);

         if (util_format_get_nr_components(buf->resources[i]->format) == 1)
            sv_templ.swizzle_r = sv_templ.swizzle_g = sv_templ.swizzle_b =
               sv_templ.swizzle_a = PIPE_SWIZZLE_X;

         buf->sampler_view_planes[i] =
            pipe->create_sampler_view(pipe, buf->resources[i], &sv_templ);
         if (!buf->sampler_view_planes[i])
            goto error;
      }
   }

   return buf->sampler_view_planes;

error:
   for (i = 0; i < buf->num_planes; ++i)
      pipe_sampler_view_reference(&buf->sampler_view_planes[i], NULL);

   return NULL;
}

static struct pipe_sampler_view **
mwv207_video_buffer_sampler_view_components(struct pipe_video_buffer *buffer)
{
   struct mwv207_video_buffer *buf = (struct mwv207_video_buffer *)buffer;
   struct pipe_sampler_view sv_templ;
   struct pipe_context *pipe;
   enum pipe_format sampler_format[VL_NUM_COMPONENTS];
   const unsigned *plane_order;
   unsigned i, j, component;

   assert(buf);

   pipe = buf->base.context;

   mwv207_get_video_buffer_formats(
      pipe->screen, buf->base.buffer_format, sampler_format);
   plane_order = mwv207_video_buffer_plane_order(buf->base.buffer_format);

   for (component = 0, i = 0; i < buf->num_planes; ++i) {
      struct pipe_resource *res = buf->resources[plane_order[i]];
      const struct util_format_description *desc =
         util_format_description(res->format);
      unsigned nr_components = util_format_get_nr_components(res->format);
      if (desc->layout == UTIL_FORMAT_LAYOUT_SUBSAMPLED)
         nr_components = 3;

      for (j = 0; j < nr_components && component < VL_NUM_COMPONENTS;
           ++j, ++component) {
         if (buf->sampler_view_components[component])
            continue;

         memset(&sv_templ, 0, sizeof(sv_templ));
         u_sampler_view_default_template(
            &sv_templ, res, sampler_format[plane_order[i]]);
         sv_templ.swizzle_r = sv_templ.swizzle_g = sv_templ.swizzle_b =
            PIPE_SWIZZLE_X + j;
         sv_templ.swizzle_a = PIPE_SWIZZLE_1;
         buf->sampler_view_components[component] =
            pipe->create_sampler_view(pipe, res, &sv_templ);
         if (!buf->sampler_view_components[component])
            goto error;
      }
   }
   assert(component == VL_NUM_COMPONENTS);

   return buf->sampler_view_components;

error:
   for (i = 0; i < VL_NUM_COMPONENTS; ++i)
      pipe_sampler_view_reference(&buf->sampler_view_components[i], NULL);

   return NULL;
}

static struct pipe_surface **
mwv207_video_buffer_surfaces(struct pipe_video_buffer *buffer)
{
   struct mwv207_video_buffer *buf = (struct mwv207_video_buffer *)buffer;
   struct pipe_surface surf_templ;
   struct pipe_context *pipe;
   unsigned i, j, array_size, surf;

   assert(buf);

   pipe = buf->base.context;

   array_size = buffer->interlaced ? 2 : 1;
   for (i = 0, surf = 0; i < VL_NUM_COMPONENTS; ++i) {
      for (j = 0; j < array_size; ++j, ++surf) {
         assert(surf < VL_MAX_SURFACES);

         if (!buf->resources[i]) {
            pipe_surface_reference(&buf->surfaces[surf], NULL);
            continue;
         }

         if (!buf->surfaces[surf]) {
            memset(&surf_templ, 0, sizeof(surf_templ));
            surf_templ.format =
               mwv207_video_buffer_surface_format(buf->resources[i]->format);
            surf_templ.u.tex.first_layer = surf_templ.u.tex.last_layer = j;
            buf->surfaces[surf] =
               pipe->create_surface(pipe, buf->resources[i], &surf_templ);
            if (!buf->surfaces[surf])
               goto error;
         }
      }
   }

   return buf->surfaces;

error:
   for (i = 0; i < VL_MAX_SURFACES; ++i)
      pipe_surface_reference(&buf->surfaces[i], NULL);

   return NULL;
}

struct pipe_video_buffer *
mwv207_video_buffer_create(struct pipe_context *pipe,
                          const struct pipe_video_buffer *tmpl)
{
   enum pipe_format resource_formats[VL_NUM_COMPONENTS];
   struct pipe_video_buffer templat, *result;
   bool pot_buffers;

   assert(pipe);
   assert(tmpl->width > 0 && tmpl->height > 0);

   pot_buffers = !pipe->screen->get_video_param(pipe->screen,
                                                PIPE_VIDEO_PROFILE_UNKNOWN,
                                                PIPE_VIDEO_ENTRYPOINT_UNKNOWN,
                                                PIPE_VIDEO_CAP_NPOT_TEXTURES);

   mwv207_get_video_buffer_formats(
      pipe->screen, tmpl->buffer_format, resource_formats);

   templat = *tmpl;
   templat.width = pot_buffers ? util_next_power_of_two(tmpl->width)
                               : align(tmpl->width, MWV207_MACROBLOCK_WIDTH);
   templat.height = pot_buffers ? util_next_power_of_two(tmpl->height)
                                : align(tmpl->height, MWV207_MACROBLOCK_HEIGHT);

   if (tmpl->interlaced)
      templat.height /= 2;

   result = mwv207_video_buffer_create_ex(pipe,
                                         &templat,
                                         resource_formats,
                                         1,
                                         tmpl->interlaced ? 2 : 1,
                                         PIPE_USAGE_DEFAULT);

   if (result && tmpl->interlaced)
      result->height *= 2;

   return result;
}

struct pipe_video_buffer *
mwv207_video_buffer_create_ex(
   struct pipe_context *pipe,
   const struct pipe_video_buffer *tmpl,
   const enum pipe_format resource_formats[VL_NUM_COMPONENTS],
   unsigned depth,
   unsigned array_size,
   unsigned usage)
{
   struct pipe_resource res_tmpl;
   struct pipe_resource *resources[VL_NUM_COMPONENTS];
   unsigned i;

   assert(pipe);

   memset(resources, 0, sizeof resources);

   mwv207_video_buffer_template(
      &res_tmpl, tmpl, resource_formats[0], depth, array_size, usage, 0);
   resources[0] = pipe->screen->resource_create(pipe->screen, &res_tmpl);
   if (!resources[0])
      goto error;

   if (resource_formats[1] == PIPE_FORMAT_NONE) {
      assert(resource_formats[2] == PIPE_FORMAT_NONE);
      return mwv207_video_buffer_create_ex2(pipe, tmpl, resources);
   }

   mwv207_video_buffer_template(
      &res_tmpl, tmpl, resource_formats[1], depth, array_size, usage, 1);
   resources[1] = pipe->screen->resource_create(pipe->screen, &res_tmpl);
   if (!resources[1])
      goto error;

   if (resource_formats[2] == PIPE_FORMAT_NONE)
      return mwv207_video_buffer_create_ex2(pipe, tmpl, resources);

   mwv207_video_buffer_template(
      &res_tmpl, tmpl, resource_formats[2], depth, array_size, usage, 2);
   resources[2] = pipe->screen->resource_create(pipe->screen, &res_tmpl);
   if (!resources[2])
      goto error;

   return mwv207_video_buffer_create_ex2(pipe, tmpl, resources);

error:
   for (i = 0; i < VL_NUM_COMPONENTS; ++i)
      pipe_resource_reference(&resources[i], NULL);

   return NULL;
}

static void
mwv207_video_buffer_resources(struct pipe_video_buffer *buffer,
                              struct pipe_resource **resources)
{
   struct mwv207_video_buffer *buf = (struct mwv207_video_buffer *)buffer;
   unsigned num_planes = util_format_get_num_planes(buffer->buffer_format);
   unsigned i;

   for (i = 0; i < num_planes; ++i)
      resources[i] = buf->resources[i];
}

struct pipe_video_buffer *
mwv207_video_buffer_create_ex2(
   struct pipe_context *pipe,
   const struct pipe_video_buffer *tmpl,
   struct pipe_resource *resources[VL_NUM_COMPONENTS])
{
   struct mwv207_video_buffer *buffer;
   unsigned i;

   buffer = CALLOC_STRUCT(mwv207_video_buffer);
   if (!buffer)
      return NULL;

   buffer->base = *tmpl;
   buffer->base.context = pipe;
   buffer->base.destroy = mwv207_video_buffer_destroy;
   /* VA frontend (vlVaGetImage/vlVaPutImage) calls get_resources() to read
    * back decoded frames; without this, vaGetImage NULL-derefs. */
   buffer->base.get_resources = mwv207_video_buffer_resources;
   buffer->base.get_sampler_view_planes =
      mwv207_video_buffer_sampler_view_planes;
   buffer->base.get_sampler_view_components =
      mwv207_video_buffer_sampler_view_components;
   buffer->base.get_surfaces = mwv207_video_buffer_surfaces;
   buffer->num_planes = 0;

   for (i = 0; i < VL_NUM_COMPONENTS; ++i) {
      buffer->resources[i] = resources[i];
      if (resources[i])
         buffer->num_planes++;
   }

   return &buffer->base;
}

struct pipe_video_buffer *
mwv207_video_buffer_create_as_resource(struct pipe_context *pipe,
                                      const struct pipe_video_buffer *tmpl)
{
   struct pipe_resource templ, *resources[VL_NUM_COMPONENTS] = {0};
   unsigned array_size = tmpl->interlaced ? 2 : 1;

   memset(&templ, 0, sizeof(templ));
   templ.target = array_size > 1 ? PIPE_TEXTURE_2D_ARRAY : PIPE_TEXTURE_2D;
   templ.width0 = align(tmpl->width, MWV207_MACROBLOCK_WIDTH);
   templ.height0 = align(tmpl->height / array_size, MWV207_MACROBLOCK_HEIGHT);
   templ.depth0 = 1;
   templ.array_size = array_size;
   templ.bind = PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET | tmpl->bind;
   templ.usage = PIPE_USAGE_DEFAULT;

   if (tmpl->buffer_format == PIPE_FORMAT_YUYV)
      templ.format = PIPE_FORMAT_R8G8_R8B8_UNORM;
   else if (tmpl->buffer_format == PIPE_FORMAT_UYVY)
      templ.format = PIPE_FORMAT_G8R8_B8R8_UNORM;
   else
      templ.format = tmpl->buffer_format;

   resources[0] = pipe->screen->resource_create(pipe->screen, &templ);
   if (!resources[0])
      return NULL;

   if (resources[0]->next) {
      pipe_resource_reference(&resources[1], resources[0]->next);
      if (resources[1]->next)
         pipe_resource_reference(&resources[2], resources[1]->next);
   }

   struct pipe_video_buffer vidtemplate = *tmpl;
   vidtemplate.width = templ.width0;
   vidtemplate.height = templ.height0 * array_size;
   return mwv207_video_buffer_create_ex2(pipe, &vidtemplate, resources);
}

struct pipe_video_buffer *
mwv207_pipe_video_buffer_create(struct pipe_context *pipe,
                               const struct pipe_video_buffer *templat)
{
   struct pipe_video_buffer templ = *templat;
   templ.bind |= PIPE_BIND_LINEAR;
   struct pipe_video_buffer *buffer = mwv207_video_buffer_create(pipe, &templ);
   struct mwv207_video_buffer *buf = (struct mwv207_video_buffer *)buffer;
   MWV207_LOG(MWV207_LEVEL_DEBUG,
             "buffer addr :%p ,buffer_format : %d  y bo handle :%d ,nv "
             "handle :%d , width :%d , height : %d\n",
             buffer,
             templat->buffer_format,
             mwv207_resource(buf->resources[0])->bo->handle,
             mwv207_resource(buf->resources[1])->bo->handle,
             buffer->width,
             buffer->height);
   return buffer;
}

struct mwv207_video_buffer *
mwv207_video_buffer_create_2(struct pipe_context *pipe,
                            unsigned size,
                            unsigned usage)
{
   MWV207_LOG(MWV207_LEVEL_LOG, "\n");
   struct mwv207_screen *jmpgu_screen = mwv207_screen(pipe->screen);
   struct mwv207_video_buffer *buffer;

   buffer = CALLOC_STRUCT(mwv207_video_buffer);

   buffer->usage = usage;

   /* Hardware buffer placement restrictions require the kernel to be
    * able to move buffers around individually, so request a
    * non-sub-allocated buffer.
    */
   buffer->res = (struct mwv207_resource *)pipe_buffer_create(
      pipe->screen, PIPE_BIND_SHARED, usage, size);

   buffer->virtual_addr =
      jmpgu_screen->ws->bo_map(jmpgu_screen->ws, buffer->res->bo);
   return buffer;
}

void
mwv207_video_buffer_alloc_buffer(struct pipe_context *pipe,
                                struct mwv207_video_buffer *buffer,
                                unsigned size,
                                unsigned usage)
{
   struct mwv207_screen *jmpgu_screen = mwv207_screen(pipe->screen);
   if (!buffer->res) {
      buffer->usage = usage;
      buffer->res = (struct mwv207_resource *)pipe_buffer_create(
         pipe->screen, PIPE_BIND_SHARED, usage, size);
      buffer->virtual_addr =
         jmpgu_screen->ws->bo_map(jmpgu_screen->ws, buffer->res->bo);
   }
}

void
mwv207_video_buffer_clear(struct pipe_context *context,
                         struct mwv207_video_buffer *buffer)
{
}

