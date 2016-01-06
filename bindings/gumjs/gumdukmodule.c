/*
 * Copyright (C) 2015 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumdukmodule.h"

#include "gumdukmacros.h"

#define GUMJS_MODULE_IMPORT_DETAILS(o) \
  ((GumImportDetails *) _gumjs_get_private_data (ctx, o))
#define GUMJS_MODULE_EXPORT_DETAILS(o) \
  ((GumExportDetails *) _gumjs_get_private_data (ctx, o))

typedef struct _GumDukMatchContext GumDukMatchContext;

struct _GumDukMatchContext
{
  GumDukModule * self;
  GumDukHeapPtr on_match;
  GumDukHeapPtr on_complete;
  duk_context * ctx;
};

GUMJS_DECLARE_CONSTRUCTOR (gumjs_module_construct)
GUMJS_DECLARE_FUNCTION (gumjs_module_enumerate_imports)
static gboolean gum_emit_import (const GumImportDetails * details,
    gpointer user_data);
GUMJS_DECLARE_FUNCTION (gumjs_module_enumerate_exports)
static gboolean gum_emit_export (const GumExportDetails * details,
    gpointer user_data);
GUMJS_DECLARE_FUNCTION (gumjs_module_enumerate_ranges)
static gboolean gum_emit_range (const GumRangeDetails * details,
    gpointer user_data);
GUMJS_DECLARE_FUNCTION (gumjs_module_find_base_address)
GUMJS_DECLARE_FUNCTION (gumjs_module_find_export_by_name)

static const duk_function_list_entry gumjs_module_functions[] =
{
  { "enumerateImports", gumjs_module_enumerate_imports, 2 },
  { "enumerateExports", gumjs_module_enumerate_exports, 2 },
  { "enumerateRanges", gumjs_module_enumerate_ranges, 3 },
  { "findBaseAddress", gumjs_module_find_base_address, 1 },
  { "findExportByName", gumjs_module_find_export_by_name, 2 },

  { NULL, NULL, 0 }
};

void
_gum_duk_module_init (GumDukModule * self,
                      GumDukCore * core)
{
  duk_context * ctx = core->ctx;

  self->core = core;

  duk_push_c_function (ctx, gumjs_module_construct, 0);
  duk_push_object (ctx);
  duk_put_function_list (ctx, -1, gumjs_module_functions);
  duk_put_prop_string (ctx, -2, "prototype");
  duk_new (ctx, 0);
  _gumjs_set_private_data (ctx, duk_require_heapptr (ctx, -1), self);
  duk_put_global_string (ctx, "Module");
}

void
_gum_duk_module_dispose (GumDukModule * self)
{
    (void) self;
}

void
_gum_duk_module_finalize (GumDukModule * self)
{
  (void) self;
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_module_construct)
{
  return 0;
}

GUMJS_DEFINE_FUNCTION (gumjs_module_enumerate_imports)
{
  GumDukMatchContext mc;
  gchar * name;
  GumDukScope scope = GUM_DUK_SCOPE_INIT (args->core);

  mc.self = _gumjs_get_private_data (ctx, _gumjs_duk_get_this (ctx));
  if (!_gumjs_args_parse (ctx, "sF{onMatch,onComplete}", &name, &mc.on_match,
      &mc.on_complete))
  {
    duk_push_null (ctx);
    return 1;
  }
  mc.ctx = ctx;

  gum_module_enumerate_imports (name, gum_emit_import, &mc);
  _gum_duk_scope_flush (&scope);

  duk_push_heapptr (ctx, mc.on_complete);
  duk_call (ctx, 0);
  duk_pop (ctx);

  return 0;
}

static gboolean
gum_emit_import (const GumImportDetails * details,
                 gpointer user_data)
{
  GumDukMatchContext * mc = user_data;
  GumDukModule * self = mc->self;
  GumDukCore * core = self->core;
  GumDukScope scope = GUM_DUK_SCOPE_INIT (core);
  duk_context * ctx = mc->ctx;
  gboolean proceed;

  duk_push_heapptr (ctx, mc->on_match);

  duk_push_object (ctx);

  if (details->type != GUM_IMPORT_UNKNOWN)
  {
    duk_push_string (ctx,
        (details->type == GUM_IMPORT_FUNCTION) ? "function" : "variable");
    duk_put_prop_string (ctx, -2, "type");
  }

  duk_push_string (ctx, details->name);
  duk_put_prop_string (ctx, -2, "name");

  if (details->module != NULL)
  {
    duk_push_string (ctx, details->module);
    duk_put_prop_string (ctx, -2, "module");
  }

  if (details->address != 0)
  {
    GumDukHeapPtr address;

    address = _gumjs_native_pointer_new (ctx,
        GSIZE_TO_POINTER (details->address), core);
    duk_push_heapptr (ctx, address);
    _gumjs_duk_release_heapptr (ctx, address);
    duk_put_prop_string (ctx, -2, "address");
  }

  if (_gum_duk_scope_call_sync (&scope, 1))
  {
    proceed = strcmp (duk_safe_to_string (ctx, -1), "stop") != 0;
  }
  else
  {
    proceed = FALSE;
  }
  duk_pop (ctx);

  return proceed;
}

GUMJS_DEFINE_FUNCTION (gumjs_module_enumerate_exports)
{
  GumDukMatchContext mc;
  gchar * name;
  GumDukScope scope = GUM_DUK_SCOPE_INIT (args->core);

  mc.self = _gumjs_get_private_data (ctx, _gumjs_duk_get_this (ctx));
  if (!_gumjs_args_parse (ctx, "sF{onMatch,onComplete}", &name, &mc.on_match,
      &mc.on_complete))
  {
    duk_push_null (ctx);
    return 1;
  }
  mc.ctx = ctx;

  gum_module_enumerate_exports (name, gum_emit_export, &mc);
  _gum_duk_scope_flush (&scope);

  duk_push_heapptr (ctx, mc.on_complete);
  duk_call (ctx, 0);
  duk_pop (ctx);

  return 0;
}

static gboolean
gum_emit_export (const GumExportDetails * details,
                 gpointer user_data)
{
  GumDukMatchContext * mc = user_data;
  GumDukModule * self = mc->self;
  GumDukCore * core = self->core;
  GumDukScope scope = GUM_DUK_SCOPE_INIT (core);
  duk_context * ctx = mc->ctx;
  GumDukHeapPtr address;
  gboolean proceed;

  duk_push_heapptr (ctx, mc->on_match);

  duk_push_object (ctx);

  duk_push_string (ctx,
      (details->type == GUM_EXPORT_FUNCTION) ? "function" : "variable");
  duk_put_prop_string (ctx, -2, "type");

  duk_push_string (ctx, details->name);
  duk_put_prop_string (ctx, -2, "name");

  address = _gumjs_native_pointer_new (ctx, GSIZE_TO_POINTER (details->address),
      core);
  duk_push_heapptr (ctx, address);
  _gumjs_duk_release_heapptr (ctx, address);
  duk_put_prop_string (ctx, -2, "address");

  if (_gum_duk_scope_call_sync (&scope, 1))
  {
    proceed = strcmp (duk_safe_to_string (ctx, -1), "stop") != 0;
  }
  else
  {
    proceed = FALSE;
  }
  duk_pop (ctx);

  return proceed;
}

GUMJS_DEFINE_FUNCTION (gumjs_module_enumerate_ranges)
{
  GumDukMatchContext mc;
  gchar * name;
  GumPageProtection prot;
  GumDukScope scope = GUM_DUK_SCOPE_INIT (args->core);

  mc.self = _gumjs_get_private_data (ctx, _gumjs_duk_get_this (ctx));
  if (!_gumjs_args_parse (ctx, "smF{onMatch,onComplete}", &name, &prot,
      &mc.on_match, &mc.on_complete))
  {
    duk_push_null (ctx);
    return 1;
  }
  mc.ctx = ctx;

  gum_module_enumerate_ranges (name, prot, gum_emit_range, &mc);
  _gum_duk_scope_flush (&scope);

  duk_push_heapptr (ctx, mc.on_complete);
  duk_call (ctx, 0);
  duk_pop (ctx);

  return 0;
}

static gboolean
gum_emit_range (const GumRangeDetails * details,
                gpointer user_data)
{
  GumDukMatchContext * mc = user_data;
  GumDukCore * core = mc->self->core;
  GumDukScope scope = GUM_DUK_SCOPE_INIT (core);
  duk_context * ctx = mc->ctx;
  char prot_str[4] = "---";
  GumDukHeapPtr range, pointer;
  gboolean proceed;

  if ((details->prot & GUM_PAGE_READ) != 0)
    prot_str[0] = 'r';
  if ((details->prot & GUM_PAGE_WRITE) != 0)
    prot_str[1] = 'w';
  if ((details->prot & GUM_PAGE_EXECUTE) != 0)
    prot_str[2] = 'x';

  duk_push_object (ctx);

  pointer = _gumjs_native_pointer_new (ctx,
      GSIZE_TO_POINTER (details->range->base_address), core);
  duk_push_heapptr (ctx, pointer);
  _gumjs_duk_release_heapptr (ctx, pointer);
  duk_put_prop_string (ctx, -2, "base");

  duk_push_uint (ctx, details->range->size);
  duk_put_prop_string (ctx, -2, "size");

  duk_push_string (ctx, prot_str);
  duk_put_prop_string (ctx, -2, "protection");

  range = _gumjs_duk_require_heapptr (ctx, -1);
  duk_pop (ctx);

  duk_push_heapptr (ctx, mc->on_match);
  duk_push_heapptr (ctx, range);

  _gumjs_duk_release_heapptr (ctx, range);

  if (_gum_duk_scope_call_sync (&scope, 1))
  {
    proceed = strcmp (duk_safe_to_string (ctx, -1), "stop") != 0;
  }
  else
  {
    proceed = FALSE;
  }
  duk_pop (ctx);

  return proceed;
}

GUMJS_DEFINE_FUNCTION (gumjs_module_find_base_address)
{
  GumDukCore * core = args->core;
  gchar * name;
  GumAddress address;
  GumDukHeapPtr result;

  if (!_gumjs_args_parse (ctx, "s", &name))
  {
    duk_push_null (ctx);
    return 1;
  }

  address = gum_module_find_base_address (name);

  if (address != 0)
  {
    result =_gumjs_native_pointer_new (ctx, GSIZE_TO_POINTER (address), core);
    duk_push_heapptr (ctx, result);
    _gumjs_duk_release_heapptr (ctx, result);
  }
  else
  {
    duk_push_null (ctx);
  }
  return 1;
}

GUMJS_DEFINE_FUNCTION (gumjs_module_find_export_by_name)
{
  GumDukCore * core = args->core;
  gchar * module_name, * symbol_name;
  GumAddress address;
  GumDukHeapPtr result;

  if (!_gumjs_args_parse (ctx, "s?s", &module_name, &symbol_name))
  {
    duk_push_null (ctx);
    return 1;
  }

  address = gum_module_find_export_by_name (module_name, symbol_name);

  if (address != 0)
  {
    result =_gumjs_native_pointer_new (ctx, GSIZE_TO_POINTER (address), core);
    duk_push_heapptr (ctx, result);
    _gumjs_duk_release_heapptr (ctx, result);
  }
  else
  {
    duk_push_null (ctx);
  }

  return 1;
}