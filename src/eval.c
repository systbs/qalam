#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <dirent.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>

#ifdef WIN32
	#include <conio.h>
#else
	#include "conio.h"
#endif

#include "types.h"
#include "utils.h"
#include "array.h"
#include "table.h"
#include "lexer.h"
#include "memory.h"
#include "object.h"
#include "variable.h"
#include "schema.h"
#include "layout.h"
#include "parser.h"
#include "data.h"


typedef struct thread {
	layout_t *layout;

	table_t *frame;
	table_t *board;

	table_t *variables;
	long_t dotted;

	object_t *object;
} thread_t;

thread_t *
thread_create(){
	thread_t *tr;
	validate_format(!!(tr = qalam_malloc(sizeof(thread_t))), 
		"unable to malloc root thread");

	tr->layout = layout_create(nullptr);

	tr->frame = table_create();
	tr->board = table_create();
	tr->dotted = 0;

	tr->variables = table_create();

	tr->object = nullptr;
	return tr;
}

//OR LOR XOR AND LAND EQ NE LT LE GT GE LSHIFT RSHIFT ADD SUB MUL DIV MOD
iarray_t *
call(thread_t *tr, schema_t *schema, iarray_t *adrs) {
	object_t *object;
	validate_format(!!(object = object_define(OTP_ADRS, sizeof(long_t))), 
		"unable to alloc memory!");
	object->ptr = (tbval_t)adrs;

	table_rpush(tr->layout->frame, (tbval_t)object);
	table_rpush(tr->board, (tbval_t)layout_create(schema));

	return (iarray_t *)schema->start;
}

iarray_t *
decode(thread_t *tr, iarray_t *c);

object_t *
import(schema_t *schema, array_t *code)
{
	thread_t *tr = thread_create();
	iarray_t *adrs = array_rpush(code, NUL);

	iarray_t *c = call(tr, schema, adrs);
	
	do {
		c = decode(tr, c);
	} while (c != code->end);

	return tr->object;
}


iarray_t *
thread_or(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");
	
	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "||")), 
			"[OR] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[OR] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "||")), 
			"[OR] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[OR] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));
	
	object_t *object;
	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	
	*(long_t *)object->ptr = oget(esp) || oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_lor(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "|")), 
			"[LOR] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[LOR] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "|")), 
			"[LOR] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[LOR] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	
	*(long_t *)object->ptr = (long_t)oget(esp) | (long_t)oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_xor(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "^")), 
			"[XOR] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[XOR] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "^")), 
			"[XOR] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[XOR] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");

	*(long_t *)object->ptr = (long_t)oget(esp) ^ (long_t)oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_and(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "&&")), 
			"[AND] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[AND] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "&&")), 
			"[AND] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[AND] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	
	*(long_t *)object->ptr = oget(esp) && oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_land(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "&")), 
			"[LAND] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[LAND] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "&")), 
			"[LAND] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[LAND] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	
	*(long_t *)object->ptr = (long_t)oget(esp) & (long_t)oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_eq(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");

	*(long_t *)object->ptr = oget(esp) == oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_ne(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	
	*(long_t *)object->ptr = (oget(esp) != oget(tr->object));

	tr->object = object;

	return c->next;
}

iarray_t *
thread_lt(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "<")), 
			"[LT] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[LT] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "<")), 
			"[LT] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[LT] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
			"unable to alloc memory!");
	*(long_t *)object->ptr = oget(esp) < oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_le(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "<=")), 
			"[LE] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[LE] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "<=")), 
			"[LE] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[LE] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	
	*(long_t *)object->ptr = oget(esp) <= oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_gt(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, ">")), 
			"[GT] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[GT] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, ">")), 
			"[GT] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[GT] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	
	*(long_t *)object->ptr = oget(esp) > oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_ge(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, ">=")), 
			"[GE] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[GE] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, ">=")), 
			"[GE] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[GE] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	
	*(long_t *)object->ptr = oget(esp) >= oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_lshift(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "<<")), 
			"[LSHIFT] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[LSHIFT] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "<<")), 
			"[LSHIFT] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[LSHIFT] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!((esp->type == OTP_LONG) && (tr->object->type == OTP_LONG)),
		"in << operator used object %s && %s", 
		object_tas(esp), object_tas(tr->object)
	);

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	
	*(long_t *)object->ptr = (long_t)oget(esp) << (long_t)oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_rshift(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, ">>")), 
			"[RSHIFT] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[RSHIFT] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, ">>")), 
			"[RSHIFT] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[RSHIFT] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!((esp->type == OTP_LONG) && (tr->object->type == OTP_LONG)),
		"in >> operator used object %s && %s", 
		object_tas(esp), object_tas(tr->object)
	);

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	
	*(long_t *)object->ptr = (long_t)oget(esp) >> (long_t)oget(tr->object);

	tr->object = object;

	return c->next;
}

iarray_t *
thread_add(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "+")), 
			"[ADD] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[ADD] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "+")), 
			"[ADD] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[ADD] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	if((esp->type == OTP_LONG) && (tr->object->type == OTP_LONG)){
		validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
			"unable to alloc memory!");
		*(long_t *)object->ptr = oget(esp) + oget(tr->object);
	} else {
		validate_format(!!(object = object_define(OTP_DOUBLE, sizeof(double_t))), 
			"unable to alloc memory!");
		*(double_t *)object->ptr = oget(esp) + oget(tr->object);
	}

	tr->object = object;

	return c->next;
}

iarray_t *
thread_sub(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "-")), 
			"[SUB] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[SUB] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "-")), 
			"[SUB] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[SUB] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	if((esp->type == OTP_LONG) && (tr->object->type == OTP_LONG)){
		validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
			"unable to alloc memory!");
		*(long_t *)object->ptr = oget(esp) - oget(tr->object);
	} else {
		validate_format(!!(object = object_define(OTP_DOUBLE, sizeof(double_t))), 
			"unable to alloc memory!");
		*(double_t *)object->ptr = oget(esp) - oget(tr->object);
	}

	tr->object = object;

	return c->next;
}

iarray_t *
thread_mul(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "*")), 
			"[MUL] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[MUL] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "*")), 
			"[MUL] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[MUL] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	if((esp->type == OTP_LONG) && (tr->object->type == OTP_LONG)){
		validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
			"unable to alloc memory!");
		*(long_t *)object->ptr = oget(esp) * oget(tr->object);
	} else {
		validate_format(!!(object = object_define(OTP_DOUBLE, sizeof(double_t))), 
			"unable to alloc memory!");
		*(double_t *)object->ptr = oget(esp) * oget(tr->object);
	}

	tr->object = object;

	return c->next;
}

iarray_t *
thread_div(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"frame is empty");

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "/")), 
			"[DIV] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[DIV] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "/")), 
			"[DIV] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[DIV] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	if((esp->type == OTP_LONG) && (tr->object->type == OTP_LONG)){
		validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
			"unable to alloc memory!");
		*(long_t *)object->ptr = (long_t)(oget(esp) / oget(tr->object));
	} else {
		validate_format(!!(object = object_define(OTP_DOUBLE, sizeof(double_t))), 
			"unable to alloc memory!");
		*(double_t *)object->ptr = (double_t)(oget(esp) / oget(tr->object));
	}

	tr->object = object;

	return c->next;
}

iarray_t *
thread_mod(thread_t *tr, iarray_t *c) {
	object_t *esp;
	if(!(esp = (object_t *)table_content(table_rpop(tr->layout->frame)))){
		printf("[%%], you can use '%%' between two object!\n");
		exit(-1);
	}

	if (esp->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)esp->ptr;

		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;
		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "%")), 
			"[MOD] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[MOD] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	else if (tr->object->type == OTP_LAYOUT) {
		layout_t *layout = (layout_t *)tr->object->ptr;
		
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);

		object_t *object = object_define(OTP_PARAMS, sizeof(table_t));
		object->ptr = tbl;

		tr->object = object;

		variable_t *var;
		validate_format(!!(var = layout_vwp(layout, "%")), 
			"[MOD] operator not defined");

		object = variable_content(var);

		validate_format(!!(object->type == OTP_SCHEMA), 
			"[MOD] operator is diffrent type of schema");

		return call(tr, (schema_t *)object->ptr, c->next);
	}

	validate_format(!!(object_isnum(esp) && object_isnum(tr->object)),
		"wrong in object type %s && %s", object_tas(esp), object_tas(tr->object));

	object_t *object;

	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
			"unable to alloc memory!");
	if((esp->type == OTP_LONG) && (tr->object->type == OTP_LONG)){
		validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
			"unable to alloc memory!");
		*(long_t *)object->ptr = (long_t)((long_t)oget(esp) % (long_t)oget(tr->object));
	} else {
		validate_format(!!(object = object_define(OTP_DOUBLE, sizeof(double_t))), 
			"unable to alloc memory!");
		*(double_t *)object->ptr = (double_t)((long_t)oget(esp) % (long_t)oget(tr->object));
	}

	tr->object = object;

	return c->next;
}

iarray_t *
thread_comma(thread_t *tr, iarray_t *c){
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"[,] you can use ',' between two object!");
	object_t *obj;
	if (esp->type != OTP_PARAMS) {
		validate_format(!!(obj = object_define(OTP_PARAMS, sizeof(table_t))), 
			"unable to alloc memory!");
		table_t *tbl = table_create();
		table_rpush(tbl, (tbval_t)esp);
		table_rpush(tbl, (tbval_t)tr->object);
		obj->ptr = tbl;
		tr->object = obj;
	} else {
		table_rpush((table_t *)esp->ptr, (tbval_t)tr->object);
		tr->object = esp;
	}
	return c->next;
}

/* 
	VAR IMM DATA SD LD PUSH JMP JZ JNZ CENT CLEV CALL ENT LEV THIS SUPER DOT
	SIZEOF TYPEOF DELETE INSERT COUNT BREAK NEW REF RET
*/

iarray_t *
thread_imm(thread_t *tr, iarray_t *c) {
	// load immediate value to object
	c = c->next;
	arval_t value = c->value;
	c = c->next;
	
	if (c->value == TP_VAR) {
		variable_t *var;
		if(tr->dotted){
			if(!!(var = layout_vwp(tr->layout, (char *)value))){
				validate_format(!!(tr->object = variable_content(var)),
					"[IMM] variable dont have content");
				return c->next;
			}
		} else if(!!(var = layout_variable(tr->layout, (char *)value))){
			validate_format(!!(tr->object = variable_content(var)),
				"[IMM] variable dont have content");
			return c->next;
		}
		schema_t *schema;
		if(!!(schema = schema_branches(tr->layout->schema, (char *)value))){
			object_t *object;
			validate_format(!!(object = object_define(OTP_SCHEMA, sizeof(schema_t))), 
				"[IMM] schema object not created");
			object->ptr = schema;
			tr->object = object;
			return c->next;
		}
		var = variable_define((char *)value);
		var->object = object_define(OTP_NULL, sizeof(ptr_t));
		table_rpush(tr->layout->variables, (tbval_t)var);
		table_rpush(tr->variables, (tbval_t)var);
		validate_format(!!(tr->object = variable_content(var)),
			"[IMM] variable dont have content");
		return c->next;
	}
	else if(c->value == TP_ARRAY){
        object_t *object;
        validate_format(!!(object = object_define(OTP_ARRAY, sizeof(table_t))), 
            "unable to alloc memory!");
        object->ptr = data_from((char_t *)value);
		tr->object = object;
		return c->next;
	}
	else if(c->value == TP_NUMBER){
        object_t *object;
		double_t num = utils_atof((char *)value);
		if(num - (long_t)num != 0){
			validate_format(!!(object = object_define(OTP_DOUBLE, sizeof(double_t))), 
            "unable to alloc memory!");
			*(double_t *)object->ptr = num;
		} else {
			validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
            "unable to alloc memory!");
			*(long_t *)object->ptr = (long_t)num;
		}
		tr->object = object;
		return c->next;
	}
	else if(c->value == TP_IMM){
        object_t *object;
        validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
            "unable to alloc memory!");
        *(long_t *)object->ptr = (long_t)value;
		tr->object = object;
		return c->next;
	}
	else if(c->value == TP_NULL){
        object_t *object;
        validate_format(!!(object = object_define(OTP_NULL, sizeof(ptr_t))), 
            "unable to alloc memory!");
        object->ptr = nullptr;
		tr->object = object;
		return c->next;
	}
	else if(c->value == TP_SCHEMA){
        object_t *object;
        validate_format(!!(object = object_define(OTP_SCHEMA, sizeof(schema_t))), 
            "unable to alloc memory!");
        object->ptr = (schema_t *)value;
		tr->object = object;
		return c->next;
	}

	validate_format( !!(c->value == TP_NULL), "[IMM] unknown type");

	return c->next;
}

iarray_t *
thread_sd(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))), 
		"save data, bad pop data");
	if(esp->type == OTP_SCHEMA){
		return call(tr, (schema_t *)esp->ptr, c->next);
	}
	else if(esp->type == OTP_PARAMS){
		validate_format((tr->object->type == OTP_PARAMS) || (tr->object->type == OTP_LAYOUT), 
			"[SD] after assign operator must be use PARAMS type");
		if(tr->object->type == OTP_LAYOUT){

			itable_t *a, *b, *e;
			table_t *ta, *tb, *tc;
			variable_t *var_a, *var_e;
			ta = (table_t *)esp->ptr;
			tb = tr->variables;

			layout_t *layout;
			layout= (layout_t *)tr->object->ptr;

			tc = layout->variables;

			a = ta->begin;
			sd_step1_layout:
			for(; a != ta->end; a = a->next){
				for(b = tb->begin; b != tb->end; b = b->next){
					var_a = (variable_t *)b->value;
					if((tbval_t)a->value == (tbval_t)var_a->object){
						goto sd_step2_layout;
					}
				}
			}
			goto sd_end_layout;

			sd_step2_layout:
			for(e = tc->begin; e != tc->end; e = e->next){
				var_e = (variable_t *)e->value;
				if(strncmp(var_a->identifier, var_e->identifier, max(strlen(var_a->identifier), strlen(var_e->identifier))) == 0){
					object_assign((object_t *)a->value, var_e->object);
					a = a->next;
					goto sd_step1_layout;
				}
			}

			a = a->next;
			goto sd_step1_layout;

			sd_end_layout:
			tr->object = esp;
			return c->next;
		}
		else {
			itable_t *a, *b, *e, *d;
			table_t *ta, *tb, *tc;
			variable_t *var_a, *var_e;
			ta = (table_t *)esp->ptr;
			tb = (table_t *)tr->variables;
			tc = (table_t *)tr->object->ptr;

			a = ta->begin;
			sd_step1:
			for(; a != ta->end; a = a->next){
				for(b = tb->begin; b != tb->end; b = b->next){
					var_a = (variable_t *)b->value;
					if((tbval_t)a->value == (tbval_t)var_a->object){
						e = tc->begin;
						goto sd_step2;
					}
				}
			}
			goto sd_end;

			sd_step2:
			for(; e != tc->end; e = e->next){
				for(d = tb->begin; d != tb->end; d = d->next){
					var_e = (variable_t *)d->value;
					if(((tbval_t)e->value == (tbval_t)var_e->object)){
						goto sd_step3;
					}
				}
			}

			a = a->next;
			goto sd_step1;

			sd_step3:
			if(strncmp(var_a->identifier, var_e->identifier, max(strlen(var_a->identifier), strlen(var_e->identifier))) == 0){
				object_assign((object_t *)a->value, (object_t *)e->value);
				a = a->next;
				goto sd_step1;
			}

			e = e->next;
			goto sd_step2;

			sd_end:
			tr->object = esp;
			return c->next;
		}
	}else {
		object_assign(esp, tr->object);
		tr->object = esp;
		return c->next;
	}
}

iarray_t *
thread_ld(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
			"bad retrive parameters");
	if(esp->type == OTP_PARAMS){
		if(tr->object->type == OTP_PARAMS){
			table_t *tt = (table_t *)tr->object->ptr;
			table_t *ts = (table_t *)esp->ptr;
			itable_t *a, *b;
			for(a = tt->begin, b = ts->begin; a != tt->end; a = a->next){
				if(b == ts->end){
					break;
				}
				object_assign((object_t *)a->value, (object_t *)b->value);
			}
		} else {
			object_t *object;
			validate_format (!!(object = (object_t *)table_content(table_lpop((table_t *)esp->ptr))),
				"bad retrive parameters");
			object_assign(tr->object, object);
		}
	} else {
		if(tr->object->type == OTP_PARAMS){
			object_t *object;
			validate_format (!!(object = (object_t *)table_content(table_lpop((table_t *)tr->object->ptr))),
				"bad retrive parameters");
			object_assign(object, esp);
		} else {
			object_assign(tr->object, esp);
		}
	}
	
	return c->next;
}

iarray_t *
thread_push(thread_t *tr, iarray_t *c) {
	// push the value of object onto the frames
	table_rpush(tr->layout->frame, (tbval_t)tr->object);
	return c->next;
}

iarray_t *
thread_jmp(thread_t *tr, iarray_t *c) {
	// jump to the address
	c = c->next;
	return (iarray_t *)c->value;
}

iarray_t *
thread_jz(thread_t *tr, iarray_t *c) {
	// jump if object is zero
	c = c->next;
	long_t res = 1;
	if(tr->object->type == OTP_PARAMS){
		itable_t * a;
		table_t *ta = tr->object->ptr;
		for(a = ta->begin; a != ta->end; a = a->next){
			object_t *obj = (object_t *)a->value;
			if(object_isnum(obj)){
				res = res && (!!oget(obj));
			} else {
				res = res && (!!obj->ptr);
			}
		}
	}
	else if(tr->object->type == OTP_LONG || tr->object->type == OTP_DOUBLE){
		res = res && ((long_t)oget(tr->object));
	}
	else {
		res = res && !!(tr->object->ptr);
	}
	return res ? c->next : (iarray_t *)c->value;
}

iarray_t *
thread_jnz(thread_t *tr, iarray_t *c) {
	// jump if object is not zero
	c = c->next;
	long_t res = 1;
	if(tr->object->type == OTP_PARAMS){
		itable_t * a;
		table_t *ta = tr->object->ptr;
		for(a = ta->begin; a != ta->end; a = a->next){
			object_t *obj = (object_t *)a->value;
			if(object_isnum(obj)){
				res = res && (!!oget(obj));
			} else {
				res = res && (!!obj->ptr);
			}
		}
	}
	else if(tr->object->type == OTP_LONG || tr->object->type == OTP_DOUBLE){
		res = res && ((long_t)oget(tr->object));
	}
	else {
		res = res && !!(tr->object->ptr);
	}
	return res ? (iarray_t *)c->value : c->next;
}



iarray_t *
thread_this(thread_t *tr, iarray_t *c) {
	object_t *object;
	validate_format(!!(object = object_define(OTP_LAYOUT, sizeof(layout_t))), 
		"unable to alloc memory!");
	object->ptr = (tbval_t)tr->layout;
	tr->object = object;
	return c->next;
}

iarray_t *
thread_super(thread_t *tr, iarray_t *c) {
	object_t *object;
	validate_format(!!(object = object_define(OTP_LAYOUT, sizeof(layout_t))), 
		"unable to alloc memory!");
	object->ptr = (tbval_t)(!!tr->layout->parent ? tr->layout->parent : tr->layout);
	tr->object = object;
	return c->next;
}


iarray_t *
thread_cell(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
			"bad get operation!");

	validate_format((esp->type == OTP_ARRAY) || (esp->type == OTP_PARAMS), 
		"[CELL] for %s type, get array operator only use for ARRAY/PARAMS type", object_tas(esp));

	validate_format(!!(tr->object->type == OTP_ARRAY), 
		"[CELL] for %s type, argument must be an array", object_tas(tr->object));

	object_t *obj;
	validate_format(!!(obj = (object_t *)table_content(table_lpop((table_t *)tr->object->ptr))), 
		"[CELL] object not retrived");

	validate_format(!!object_isnum(obj), 
		"[CELL] for %s type, index must be a number type", object_tas(obj));

	tr->object = (object_t *)table_content(table_at((table_t *)esp->ptr, oget(obj)));
	return c->next;
}

iarray_t *
thread_call(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"bad call operation!");

	validate_format((esp->type == OTP_SCHEMA), 
		"[CALL] for %s type, eval operator only use for SCHEMA type", object_tas(esp));
		
	return call(tr, (schema_t *)esp->ptr, c->next);
}

iarray_t *
thread_ent(thread_t *tr, iarray_t *c) {
	table_rpush(tr->frame, (tbval_t)tr->layout);

	layout_t *layout;
	layout = (layout_t *)table_content(table_rpop(tr->board));
	layout->parent = (layout_t *)layout->schema->root;
	tr->layout = layout;

	tr->layout->object = object_define(OTP_NULL, sizeof(ptr_t));

	return c->next;
}

iarray_t *
thread_extend(thread_t *tr, iarray_t *c){
	itable_t *b, *n;
	for(b = tr->layout->schema->extends->begin; b != tr->layout->schema->extends->end; b = b->next){
		schema_t *schema = (schema_t *)b->value;
		int found = 0;
		for(n = tr->layout->extends->begin; n != tr->layout->extends->end; n = n->next){
			layout_t *layout = (layout_t *)n->value;
			if(strncmp(layout->schema->identifier, schema->identifier, max(strlen(layout->schema->identifier), strlen(schema->identifier))) == 0){
				found = 1;
				break;
			}
		}
		if(found){
			continue;
		}
		layout_t *layout = layout_create(schema);
		object_t *object;
		validate_format(!!(object = object_define(OTP_ADRS, sizeof(long_t))), 
			"unable to alloc memory!");
		object->ptr = (tbval_t)c;
		table_rpush(tr->layout->frame, (tbval_t)object);
		table_rpush(tr->layout->extends, (tbval_t)layout);
		table_rpush(tr->board, (tbval_t)layout);
		return (iarray_t *)schema->start;
	}
	
	return c->next;
}

iarray_t *
thread_lev(thread_t *tr, iarray_t *c) {
	tr->object = tr->layout->object;
	tr->layout = (layout_t *)table_content(table_rpop(tr->frame));

	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"wrong! bad leave function, not found adrs");

	validate_format((esp->type == OTP_ADRS), 
		"wrong! object not from type address");
	
	iarray_t * adrs;
	adrs = (iarray_t *)esp->ptr;

	return adrs;
}

iarray_t *
thread_ret(thread_t *tr, iarray_t *c) {
	tr->layout->object = tr->object;
	return c->next;
}

iarray_t *
thread_scheming(thread_t *tr, iarray_t *c) {
	object_t *esp;
	validate_format(!!(esp = (object_t *)table_content(table_rpop(tr->layout->frame))),
		"[CLS] missing object");

	validate_format((esp->type == OTP_SCHEMA) || (esp->type == OTP_NULL) || (esp->type == OTP_ARRAY), 
		"[CLS] scheming type use only for null or schema type, %s", object_tas(esp));

	if((esp->type == OTP_ARRAY)){
		char_t *schema_name = data_to((table_t *)esp->ptr);
		
		validate_format(!!(esp = object_redefine(esp, OTP_SCHEMA, sizeof(schema_t))), 
			"[CLS] redefine object for type schema");

		variable_t *var;
		var = variable_define(schema_name);
		var->object = esp;
		table_rpush(tr->layout->variables, (tbval_t)var);
		table_rpush(tr->variables, (tbval_t)var);
	}

	if((esp->type == OTP_NULL)){
		validate_format(!!(esp = object_redefine(esp, OTP_SCHEMA, sizeof(schema_t))), 
			"[CLS] redefine object for type schema");
		if(tr->object->type == OTP_PARAMS) {
			object_t *object;
			validate_format(!!(object = (object_t *)table_content(table_rpop((table_t *)tr->object->ptr))), 
				"[CLS] pop schema fram list ignored");
			validate_format((object->type == OTP_SCHEMA), 
				"[CLS] scheming type use only for schema type, %s", 
				object_tas(object)
			);
			esp->ptr = object->ptr;
		} else {
			validate_format((tr->object->type == OTP_SCHEMA), 
				"[CLS] scheming type use only for schema type, %s", object_tas(tr->object)
			);
			esp->ptr = tr->object->ptr;
			schema_t *schema;
			validate_format(!!(schema = (schema_t *)esp->ptr),
				"[CLS] schema is null");
			schema->root = tr->layout;
			tr->object = esp;
			return c->next;
		}
	}

	schema_t *schema;
	validate_format(!!(schema = (schema_t *)esp->ptr),
		"[CLS] schema is null");

	schema->root = tr->layout;

	if(tr->object->type == OTP_PARAMS){
		table_t *tbl = (table_t *)tr->object->ptr;
		itable_t *b;
		for(b = tbl->begin; b != tbl->end; b = b->next){
			object_t *object = (object_t *)b->value;
			validate_format((object->type == OTP_SCHEMA), 
				"[CLS] schema type for scheming operator is required %s", object_tas(object));
			table_rpush(schema->extends, (tbval_t)object->ptr);
		}
	} 
	else if(tr->object->type == OTP_SCHEMA) {
		validate_format((tr->object->type == OTP_SCHEMA), 
			"[CLS] schema type for scheming operator is required %s", object_tas(tr->object));
		table_rpush(schema->extends, (tbval_t)tr->object->ptr);
	}

	tr->object = esp;
	return c->next;
}

iarray_t *
thread_sim(thread_t *tr, iarray_t *c){
	validate_format(!!((tr->object->type == OTP_SCHEMA) || (tr->object->type == OTP_LAYOUT)), 
		"[SIM] object is empty");
	tr->dotted = 1;
	if(tr->object->type == OTP_SCHEMA){
		schema_t *schema;
		validate_format(!!(schema = (schema_t *)tr->object->ptr), 
			"[SIM] object is empty");
		table_rpush(tr->frame, (tbval_t)tr->layout);
		tr->layout = layout_create(schema);
		return c->next;
	}
	else {
		layout_t *layout;
		validate_format(!!(layout = (layout_t *)tr->object->ptr), 
			"[SIM] object is empty");
		table_rpush(tr->frame, (tbval_t)tr->layout);
		tr->layout = layout;
		return c->next;
	}
}

iarray_t *
thread_rel(thread_t *tr, iarray_t *c){
	tr->dotted = 0;
	validate_format(!!(tr->layout = (layout_t *)table_content(table_rpop(tr->frame))), 
		"layout rel back frame is empty");
	return c->next;
}

iarray_t *
thread_print(thread_t *tr, iarray_t *c) {
	table_t *tbl;
	if(tr->object->type == OTP_PARAMS){
		tbl = (table_t *)tr->object->ptr;
	} else {
		tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);
	}

	object_t *eax;
	while((eax = (object_t *)table_content(table_lpop(tbl)))){
		if(eax->type == OTP_NULL){
			printf("%s ", object_tas(eax));
			continue;
		}
		else if(eax->type == OTP_CHAR){
			printf("%d ", *(char_t *)eax->ptr);
			continue;
		}
		else if(eax->type == OTP_ARRAY){
			table_t *formated;

			formated = data_format((table_t *)eax->ptr, tbl);

			itable_t *t;
			while((t = table_lpop(formated))){
				object_t *obj = (object_t *)t->value;
				if(*(char_t *)obj->ptr == '\\'){

					t = table_lpop(formated);
					obj = (object_t *)t->value;

					if(*(char_t *)obj->ptr == 'n'){
						printf("\n");
						continue;
					}
					else
					if(*(char_t *)obj->ptr == 't'){
						printf("\t");
						continue;
					}
					else
					if(*(char_t *)obj->ptr == 'v'){
						printf("\v");
						continue;
					}
					else
					if(*(char_t *)obj->ptr == 'a'){
						printf("\a");
						continue;
					}
					else
					if(*(char_t *)obj->ptr == 'b'){
						printf("\b");
						continue;
					}

					printf("\\");
					continue;
				}

				printf("%c", *(char_t *)obj->ptr);
			}

			continue;
		}
		else if(eax->type == OTP_LONG){
			printf("%ld", *(long_t *)eax->ptr);
			continue;
		}
		else if(eax->type == OTP_DOUBLE){
			printf("%.6f", *(double_t *)eax->ptr);
			continue;
		}
		else if(eax->type == OTP_SCHEMA) {
			printf("SCHEMA \n");
		}
		else {
			printf("%ld", *(long_t *)eax->ptr);
		}
	}
	return c->next;
}

iarray_t *
thread_bscp(thread_t *tr, iarray_t *c) {
	table_rpush(tr->layout->scope, (tbval_t)tr->layout->variables);
	tr->layout->variables = table_create();
	return c->next;
}

iarray_t *
thread_escp(thread_t *tr, iarray_t *c) {
	tr->layout->variables = (table_t *)table_content(table_rpop(tr->layout->scope));
	return c->next;
}

iarray_t *
thread_array(thread_t *tr, iarray_t *c){
	validate_format(!!tr->object,
		"[ARRAY] object is required!");
	object_t *obj;
	validate_format(!!(obj = object_define(OTP_ARRAY, sizeof(table_t))), 
		"unable to alloc memory!");
	table_t *tbl = table_create();
	if(tr->object->type == OTP_PARAMS){
		table_t *tt = (table_t *)tr->object->ptr;
		itable_t *a;
		for(a = tt->begin; a != tt->end; a = a->next){
			table_rpush(tbl, (tbval_t)a->value);
		}
	} else {
		table_rpush(tbl, (tbval_t)tr->object);
	}
	obj->ptr = tbl;
	tr->object = obj;
	return c->next;
}

iarray_t *
thread_params(thread_t *tr, iarray_t *c){
	validate_format(!!tr->object,
		"[PARAMS] object is required!");
	object_t *obj;
	validate_format(!!(obj = object_define(OTP_PARAMS, sizeof(table_t))), 
		"unable to alloc memory!");
	table_t *tbl = table_create();
	if(tr->object->type == OTP_PARAMS){
		table_t *tt = (table_t *)tr->object->ptr;
		itable_t *a;
		for(a = tt->begin; a != tt->end; a = a->next){
			table_rpush(tbl, (tbval_t)a->value);
		}
	} else {
		table_rpush(tbl, (tbval_t)tr->object);
	}
	obj->ptr = tbl;
	tr->object = obj;
	return c->next;
}

iarray_t *
thread_import(thread_t *tr, iarray_t *c){
	validate_format(!!(tr->object->type == OTP_ARRAY), 
		"[IMPORT] import function need to an array");

	char *destination;
	destination = data_to((table_t *)tr->object->ptr);

    FILE *fd;
	validate_format (!!(fd = fopen(destination, "rb")),
		"[IMPORT] could not open(%s)\n", destination);

    // Current position
    long_t pos = ftell(fd);
    // Go to end
    fseek(fd, 0, SEEK_END);
    // read the position which is the size
    long_t chunk = ftell(fd);
    // restore original position
    fseek(fd, pos, SEEK_SET);

    char *buf;

    validate_format (!!(buf = malloc(chunk + 1)),
		"could not malloc(%ld) for buf area\n", chunk);

    // read the source file
	long_t i = fread(buf, 1, chunk, fd);
    validate_format (!!(i >= chunk), "read returned %ld\n", i);

    buf[i] = '\0';

    fclose(fd);

	table_t *tokens;
	validate_format(!!(tokens = table_create()), 
		"[IMPORT] tokens list not created");

    lexer(tokens, buf);

    qalam_free(buf);

    array_t *code;
	validate_format(!!(code = array_create()), 
		"[IMPORT] code array not created");
	
    parser_t *parser;
	validate_format(!!(parser = parse(tokens, code)), 
		"[IMPORT] parser is aborted");

    tr->object = import(parser->schema, code);

	return c->next;
}

iarray_t *
thread_fork(thread_t *tr, iarray_t *c) {
	object_t *object;
	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	*(long_t *)object->ptr = fork();
	tr->object = object;
	return c->next;
}

iarray_t *
thread_getpid(thread_t *tr, iarray_t *c) {
	object_t *object;
	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	*(long_t *)object->ptr = (long_t)getpid();
	tr->object = object;
	return c->next;
}

iarray_t *
thread_wait(thread_t *tr, iarray_t *c) {
	object_t *object;
	validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
		"unable to alloc memory!");
	*(long_t *)object->ptr = wait(nullptr);
	tr->object = object;
	return c->next;
}

iarray_t *
thread_sleep(thread_t *tr, iarray_t *c) {
	table_t *tbl;
	if(tr->object->type == OTP_PARAMS){
		tbl = (table_t *)tr->object->ptr;
	} else {
		tbl = table_create();
		table_rpush(tbl, (tbval_t)tr->object);
	}

	table_t *tblresp;
	tblresp = table_create();

	object_t *eax;
	while((eax = (object_t *)table_content(table_lpop(tbl)))){
		validate_format((eax->type == OTP_LONG), "Not true validated");
		object_t *object;
		validate_format(!!(object = object_define(OTP_LONG, sizeof(long_t))), 
			"unable to alloc memory!");
		*(long_t *)object->ptr = (long_t)sleep((unsigned int)(*(long_t *)eax->ptr));
		table_rpush(tblresp, (tbval_t)object);
	}

	if(table_count(tblresp) > 1){
		object_t *object;
		validate_format(!!(object = object_define(OTP_PARAMS, sizeof(table_t))), 
			"unable to alloc memory!");
		object->ptr = tblresp;
		tr->object = object;
	} else {
		tr->object = (object_t *)table_content(table_rpop(tblresp));
	}

	return c->next;
}

iarray_t *
decode(thread_t *tr, iarray_t *c) {

	// printf("thread %ld\n", c->value);

	switch (c->value) {
		case NUL:
			return c->next;
			break;
		case BLP:
			return c->next;
			break;
		case ELP:
			return c->next;
			break;
		case BSCP:
			return thread_bscp(tr, c);
			break;
		case ESCP:
			return thread_escp(tr, c);
			break;

		case OR:
			return thread_or(tr, c);
			break;
		case LOR:
			return thread_lor(tr, c);
			break;
		case XOR:
			return thread_xor(tr, c);
			break;
		case AND:
			return thread_and(tr, c);
			break;
		case LAND:
			return thread_land(tr, c);
			break;
		case EQ:
			return thread_eq(tr, c);
			break;
		case NE:
			return thread_ne(tr, c);
			break;
		case LT:
			return thread_lt(tr, c);
			break;
		case LE:
			return thread_le(tr, c);
			break;
		case GT:
			return thread_gt(tr, c);
			break;
		case GE:
			return thread_ge(tr, c);
			break;
		case LSHIFT:
			return thread_lshift(tr, c);
			break;
		case RSHIFT:
			return thread_rshift(tr, c);
			break;
		case ADD:
			return thread_add(tr, c);
			break;
		case SUB:
			return thread_sub(tr, c);
			break;
		case MUL:
			return thread_mul(tr, c);
			break;
		case DIV:
			return thread_div(tr, c);
			break;
		case MOD:
			return thread_mod(tr, c);
			break;

			// VAR IMM DATA SD LD PUSH JMP JZ JNZ CENT CLEV CALL ENT LEV THIS SUPER DOT
			// SIZEOF TYPEOF DELETE INSERT COUNT BREAK NEW REF RET
		case IMM:
			return thread_imm(tr, c);
			break;
		case SD:
			return thread_sd(tr, c);
			break;
		case LD:
			return thread_ld(tr, c);
			break;
		case PUSH:
			return thread_push(tr, c);
			break;
		case JMP:
			return thread_jmp(tr, c);
			break;
		case JZ:
			return thread_jz(tr, c);
			break;
		case JNZ:
			return thread_jnz(tr, c);
			break;
		case COMMA:
			return thread_comma(tr, c);
			break;
		case CALL:
			return thread_call(tr, c);
			break;
		case CELL:
			return thread_cell(tr, c);
			break;
		case ENT:
			return thread_ent(tr, c);
			break;
		case EXD:
			return thread_extend(tr, c);
			break;
		case LEV:
			return thread_lev(tr, c);
			break;
		case REL:
			return thread_rel(tr, c);
			break;
		case SIM:
			return thread_sim(tr, c);
			break;
		case CLS:
			return thread_scheming(tr, c);
			break;
		case RET:
			return thread_ret(tr, c);
			break;

		case THIS:
			return thread_this(tr, c);
			break;
		case SUPER:
			return thread_super(tr, c);
			break;
		case PRTF:
			return thread_print(tr, c);
			break;
		case ARRAY:
			return thread_array(tr, c);
			break;
		case PARAMS:
			return thread_params(tr, c);
			break;
		case IMPORT:
			return thread_import(tr, c);
			break;
		case FORK:
			return thread_fork(tr, c);
			break;
		case WAIT:
			return thread_wait(tr, c);
			break;
		case GETPID:
			return thread_getpid(tr, c);
			break;
		case SLEEP:
			return thread_sleep(tr, c);
			break;
		case EXIT:
			exit(0);
			return c->next;
			break;

		default:
			printf("unknown instruction %ld\n", c->value);
            exit(-1);
	}

	return c;
}


void
eval(schema_t *root, array_t *code)
{
	thread_t *tr = thread_create();

	iarray_t *adrs = array_rpush(code, EXIT);

	root->root = tr->layout;

	iarray_t *c = call(tr, root, adrs);

	do {
		c = decode(tr, c);
	} while (c != code->end);
}




int
main(int argc, char **argv)
{
	argc--;
    argv++;

    // parse arguments
    if (argc < 1) {
        printf("usage: file ...\n");
        return -1;
    }

    arval_t i;
    FILE *fd;

	char destination [ MAX_PATH ];

#ifdef WIN32
	if(**argv != '\\'){
#else
	if(**argv != '/'){
#endif
		char cwd[MAX_PATH];
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			perror("getcwd() error");
			return -1;
		}
		utils_combine ( destination, cwd, *argv );
	} else {
		strcpy(destination, *argv);
	}

    if (!(fd = fopen(destination, "rb"))) {
        printf("could not open(%s)\n", destination);
        return -1;
    }

    argc--;
    argv++;

    // Current position
    arval_t pos = ftell(fd);
    // Go to end
    fseek(fd, 0, SEEK_END);
    // read the position which is the size
    arval_t chunk = ftell(fd);
    // restore original position
    fseek(fd, pos, SEEK_SET);

    char *buf;

    if (!(buf = malloc(chunk + 1))) {
        printf("could not malloc(%ld) for buf area\n", chunk);
        return -1;
    }

    // read the source file
    if ((i = fread(buf, 1, chunk, fd)) < chunk) {
        printf("read returned %ld\n", i);
        return -1;
    }

    buf[i] = '\0';

    fclose(fd);

    table_t *tokens = table_create();
    lexer(tokens, buf);
    qalam_free(buf);

    array_t *code = array_create();
    parser_t *parser = parse(tokens, code);

    eval(parser->schema, code);

    return 0;
}
