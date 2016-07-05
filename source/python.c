/*
 * python.c -- Calling Python from epic.
 *
 * Copyright 2016 EPIC Software Labs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, the above paragraph (the one permitting redistribution),
 *    this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* Python commit #10 */

#include <Python.h>
#include "irc.h"
#include "ircaux.h"
#include "array.h"
#include "alias.h"
#include "commands.h"
#include "functions.h"
#include "output.h"
#include "ifcmd.h"
#include "extlang.h"

void	output_traceback (void);

static	int	p_initialized = 0;
static	PyObject *global_vars = NULL;

/*
 * ObRant
 *
 * So Python is a language like ircII where there is a distinction made between
 * "statements" and "expressions".  In both languages, a "statement" is a call
 * that does not result in a return value, and expression is.  I can't blame 
 * python for this.  But it is different from Perl, Ruby, and TCL, which treat
 * everything as a "callable" and then you either get a return value (for an 
 * expression) or an empty string (for a statement).
 *
 * In order to make python support work, though, we have to honor this distinction.
 * I've chosen to do this through the /PYTHON command and the $python() function
 *
 * 	You can only use the /PYTHON command to run statements.
 *	Using /PYTHON with an expression will result in an exception being thrown.
 *
 *	You can only use the $python() function to evaluate expressions
 *	Using $python() with a statement will result in an exception being thrown.
 *
 * How do you know whether what you're doing is an expression or a statement, when
 * if you just throw everything into one file you don't have to worry about it?
 * Good question.  I don't know.  Good luck!
 */

/*
 * I owe many thanks to 
 *	https://docs.python.org/3.6/extending/embedding.html
 *	https://docs.python.org/3.6/c-api/veryhigh.html
 *	https://docs.python.org/3/c-api/init.html
 *	https://docs.python.org/3/c-api/exceptions.html
 *	https://www6.software.ibm.com/developerworks/education/l-pythonscript/l-pythonscript-ltr.pdf
 *	http://boost.cppll.jp/HEAD/libs/python/doc/tutorial/doc/using_the_interpreter.html
 *	  (for explaining what the "start" flag is to PyRun_String)
 * for teaching how to embed and extend Python in a C program.
 */

/*
 * I owe many many thanks to skully for working with me on this
 * and giving me good advice on how to make this not suck.
 */

/*************************************************************************/
/*
 * Extended Python by creating an "import epic" module.
 */
/*
 * Psuedo-code of how to create a python module that points at your C funcs
 *
 * These functions provide the basic facilities of ircII.  
 * All of these functions take one string argument.
 *   epic.echo	- yell() -- Like /echo, unconditionally output to screen
 *   epic.say	- say()	 -- Like /xecho -s, output if not suppressed with ^
 *   epic.cmd	- runcmds() -- Run a block of code, but don't expand $'s (like from the input line)
 *   epic.eval	- runcmds() -- Run a block of code, expand $'s, but $* is []  (this is lame)
 *   epic.expr	- parse_inline() -- Evaluate an expression string and return the result 
 *   epic.call	- call_function() -- Evaluate a "funcname(argument list)" string and return the result
 *
 * These functions provide a route-around of ircII, if that's what you want.
 * All of these functions take a symbol name ("name") and a string containing the arguments (if appropriate)
 * NONE OF THESE FUNCTIONS UNDERGO $-EXPANSION!  (If you need that, use the stuff ^^^ above)
 *
 *   epic.run_command - run an alias (preferentially) or a builtin command.
 *        Example: epic.run_command("xecho", "-w 0 hi there!  This does not get expanded")
 *   epic.call_function - call an alias (preferentially) or a builtin function
 *        Example: epic.call_function("windowctl", "refnums")
 *   epic.get_set - get a /SET value (only)
 *        Example: epic.get_set("mail")
 *   epic.get_assign - get an /ASSIGN value (only)
 *        Example: epic.get_set("myvar")
 *   epic.get_var - get an /ASSIGN value (preferentially) or a /SET 
 *   epic.set_set - set a /SET value (only)
 *        Example: epic.set_set("mail", "ON")
 *   epic.set_assign - set a /ASSIGN value
 *        Example: epic.set_assign("myvar", "5")
 */

/* Higher level interface to things */
static	PyObject *	epic_echo (PyObject *self, PyObject *args)
{
	char *	str;

	if (!PyArg_ParseTuple(args, "z", &str)) {
		return PyLong_FromLong(-1);
	}

	yell(str);
	return PyLong_FromLong(0L);
}

static	PyObject *	epic_say (PyObject *self, PyObject *args)
{
	char *	str;

	if (!PyArg_ParseTuple(args, "z", &str)) {
		return PyLong_FromLong(-1);
	}

	say(str);
	return PyLong_FromLong(0L);
}

static	PyObject *	epic_cmd (PyObject *self, PyObject *args)
{
	char *	str;

	if (!PyArg_ParseTuple(args, "z", &str)) {
		return PyLong_FromLong(-1);
	}

	runcmds("$*", str);
	return PyLong_FromLong(0L);
}

static	PyObject *	epic_eval (PyObject *self, PyObject *args)
{
	char *	str;

	if (!PyArg_ParseTuple(args, "z", &str)) {
		return PyLong_FromLong(-1);
	}

	runcmds(str, "");
	return PyLong_FromLong(0L);
}

static	PyObject *	epic_expr (PyObject *self, PyObject *args)
{
	char *	str;
	char *	exprval;
	PyObject *retval;

	if (!PyArg_ParseTuple(args, "z", &str)) {
		return PyLong_FromLong(-1);
	}

	exprval = parse_inline(str, "");
	if (!(retval = Py_BuildValue("z", exprval))) {
		yell("epic_expr: Py_BuildValue failed. hrm.");
		return NULL;
	}
	new_free(&exprval);
	return retval;
}

static	PyObject *	epic_expand (PyObject *self, PyObject *args)
{
	char *	str;
	char *	expanded;
	PyObject *retval;

	if (!PyArg_ParseTuple(args, "z", &str)) {
		return PyLong_FromLong(-1);
	}

	expanded = expand_alias(str, "");
	retval = Py_BuildValue("z", expanded);
	new_free(&expanded);
	return retval;
}

static	PyObject *	epic_call (PyObject *self, PyObject *args)
{
	char *	str;
	char *	funcval;
	PyObject *retval;

	if (!PyArg_ParseTuple(args, "z", &str)) {
		return PyLong_FromLong(-1);
	}

	funcval = call_function(str, "");
	retval = Py_BuildValue("z", funcval);
	new_free(&funcval);
	return retval;
}

/* Lower level interfaces to things */
static	PyObject *	epic_run_command (PyObject *self, PyObject *args)
{
	char *	symbol;
	char *	my_args;
	PyObject *retval;
	void    (*builtin) (const char *, char *, const char *) = NULL;
const 	char *	alias = NULL;
	void *	arglist = NULL;

	if (!PyArg_ParseTuple(args, "zz", &symbol, &my_args)) {
		return PyLong_FromLong(-1);
	}

	upper(symbol);
	alias = get_cmd_alias(symbol, &arglist, &builtin);
	if (alias) 
		call_user_command(symbol, alias, my_args, arglist);
	else if (builtin)
		builtin(symbol, my_args, empty_string);

	/* Call /symbol args */
	return PyLong_FromLong(0L);
}


static	PyObject *	epic_call_function (PyObject *self, PyObject *args)
{
	char *	symbol;
	char *	my_args;
	PyObject *retval;
	const char *alias;
	void *	arglist;
	char * (*builtin) (char *) = NULL;
	char *	funcval = NULL;

	if (!PyArg_ParseTuple(args, "zz", &symbol, &my_args)) {
		return PyLong_FromLong(-1);
	}

	upper(symbol);
        alias = get_func_alias(symbol, &arglist, &builtin);
        if (alias)
                funcval = call_user_function(symbol, alias, my_args, arglist);
	else if (builtin)
                funcval = builtin(my_args);
	else
		funcval = malloc_strdup(empty_string);

	retval = Py_BuildValue("z", funcval);
	new_free(&funcval);
	return retval;
}

static	PyObject *	epic_get_set (PyObject *self, PyObject *args)
{
	char *	symbol;
	PyObject *retval;
	char *	funcval = NULL;

	if (!PyArg_ParseTuple(args, "z", &symbol)) {
		return PyLong_FromLong(-1);
	}
 
	upper(symbol);
	funcval = make_string_var(symbol);
	retval = Py_BuildValue("z", funcval);
	new_free(&funcval);
	return retval;
}

static	PyObject *	epic_get_assign (PyObject *self, PyObject *args)
{
	char *	symbol;
	PyObject *retval;
	char *	funcval = NULL;
        char *		(*efunc) (void) = NULL;
        IrcVariable *	sfunc = NULL;
	const char *	assign = NULL;

	if (!PyArg_ParseTuple(args, "z", &symbol)) {
		return PyLong_FromLong(-1);
	}

	upper(symbol);
        assign = get_var_alias(symbol, &efunc, &sfunc);
	retval = Py_BuildValue("z", assign);
	/* _DO NOT_ FREE 'assign'! */
	return retval;
}

/*  
 * XXX Because of how this is implemented, this supports ":local" and "::global", although 
 * I'm not sure it will stay this way forever.
 */
static	PyObject *	epic_get_var (PyObject *self, PyObject *args)
{
	char *	symbol;
	PyObject *retval;
	char *	funcval = NULL;

	if (!PyArg_ParseTuple(args, "z", &symbol)) {
		return PyLong_FromLong(-1);
	}

	upper(symbol);
	funcval = get_variable(symbol); 
	retval = Py_BuildValue("z", funcval);
	new_free(&funcval);
	return retval;
}

static	PyObject *	epic_set_set (PyObject *self, PyObject *args)
{
	char *	symbol;
	char *	value;
	PyObject *retval;

	if (!PyArg_ParseTuple(args, "zz", &symbol, &value)) {
		return PyLong_FromLong(-1);
	}

	/* /SET symbol value */
	return PyLong_FromLong(0);
}

static	PyObject *	epic_set_assign (PyObject *self, PyObject *args)
{
	char *	symbol;
	char *	value;
	PyObject *retval;

	if (!PyArg_ParseTuple(args, "zz", &symbol, &value)) {
		return PyLong_FromLong(-1);
	}

	/* /ASSIGN symbol value */
	return PyLong_FromLong(0);
}


static	PyMethodDef	epicMethods[] = {
      /* Higher level facilities  - $-expansion supported */
	{ "echo", 	   epic_echo, 	METH_VARARGS, 	"Unconditionally output to screen (yell)" },
	{ "say", 	   epic_say, 	METH_VARARGS, 	"Output to screen unless suppressed (say)" },
	{ "cmd", 	   epic_cmd, 	METH_VARARGS, 	"Run a block statement without expansion (runcmds)" },
	{ "eval", 	   epic_eval, 	METH_VARARGS, 	"Run a block statement with expansion (but $* is empty)" },
	{ "expr", 	   epic_expr, 	METH_VARARGS, 	"Return the result of an expression (parse_inline)" },
	{ "call", 	   epic_call, 	METH_VARARGS, 	"Call a function with expansion (but $* is empty) (call_function)" },
	{ "expand",	   epic_expand,	METH_VARARGS,	"Expand some text with $s" },

      /* Lower level facilities - $-expansion NOT supported */
	{ "run_command",   epic_run_command,	METH_VARARGS,	"Run an alias or builtin command" },
	{ "call_function", epic_call_function,	METH_VARARGS,	"Call an alias or builtin function" },
	{ "get_set",       epic_get_set,	METH_VARARGS,	"Get a /SET value (only)" },
	{ "get_assign",    epic_get_assign,	METH_VARARGS,	"Get a /ASSIGN value (only)" },
	{ "get_var",       epic_get_assign,	METH_VARARGS,	"Get a variable (either /ASSIGN or /SET)" },
	{ "set_set",       epic_set_set,	METH_VARARGS,	"Set a /SET value (only)" },
	{ "set_assign",    epic_set_assign,	METH_VARARGS,	"Set a /ASSIGN value (only)" },

	{ NULL,		NULL,		0,		NULL }
};

static	PyModuleDef	epicModule = {
	PyModuleDef_HEAD_INIT,	
	"_epic", 	NULL, 		-1,		epicMethods,
	NULL,		NULL,		NULL,		NULL
};

/* 
 * This is called via
 *	PyImport_AppendInittab("_epic", &PyInit_epic);
 * before
 *	PyInitialize();
 * in main().
 */
static	PyObject *	PyInit_epic (void)
{
	return PyModule_Create(&epicModule);
}


/***********************************************************/
/* 
 * Embed Python by allowing users to call a python function
 */
char *	python_eval_expression (char *input)
{
	PyObject *retval;
	PyObject *retval_repr;
	char 	*r, *retvalstr = NULL;

	if (p_initialized == 0)
	{
		PyImport_AppendInittab("_epic", &PyInit_epic);
		Py_Initialize();
		p_initialized = 1;
		global_vars = PyModule_GetDict(PyImport_AddModule("__main__"));
	}

	/* Convert retval to a string */
	retval = PyRun_String(input, Py_eval_input, global_vars, global_vars);
	if (retval == NULL)
		output_traceback();

	retval_repr = PyObject_Repr(retval);
	r = PyUnicode_AsUTF8(retval_repr);
	retvalstr = malloc_strdup(r);

	Py_XDECREF(retval);
	Py_XDECREF(retval_repr);
	RETURN_MSTR(retvalstr);	
}

void	python_eval_statement (char *input)
{
	int	retval;

	if (p_initialized == 0)
	{
		PyImport_AppendInittab("_epic", &PyInit_epic);
		Py_Initialize();
		p_initialized = 1;
		global_vars = PyModule_GetDict(PyImport_AddModule("__main__"));
	}

	/* 
	 * XXX - This outputs to stdout (yuck) on exception.
	 * I need to figure out how to defeat that.
	 */
	if ((retval = PyRun_SimpleString(input)))
		output_traceback();
}


/*
 * The /PYTHON command: Evalulate the args as a PYTHON block and ignore the 
 * return value of the statement.
 */
BUILT_IN_COMMAND(pythoncmd)
{
        char *body;

        if (*args == '{')
        {
                if (!(body = next_expr(&args, '{')))
                {
                        my_error("PYTHON: unbalanced {");
                        return;
                }
        }
        else
                body = args;

        python_eval_statement(body);
}

/*
 * The /PYDIRECT command.  Call a python module.method directly without quoting hell
 *  The return value (if any) is ignored.
 */
char *	call_python_directly (char *args)
{
	char 	*object = NULL, *module = NULL, *method = NULL;
	PyObject *mod_py = NULL, *meth_py = NULL, *args_py = NULL;
	PyObject *pModule = NULL, *pFunc = NULL, *pArgs = NULL, *pRetVal = NULL;
	PyObject *retval_repr = NULL;
	char 	*r = NULL, *retvalstr = NULL;

	if (!(object = new_next_arg(args, &args)))
	{
		my_error("Usage: /PYDIRECT module.method arguments");
		RETURN_EMPTY;
	}

	module = object;
	if (!(method = strchr(module, '.')))
	{
		my_error("Usage: /PYDIRECT module.method arguments");
		RETURN_EMPTY;
	}
	*method++ = 0;

	mod_py = Py_BuildValue("z", module);
	pModule = PyImport_Import(mod_py);
	Py_XDECREF(mod_py);

	if (pModule == NULL)
	{
		my_error("PYDIRECT: Unable to import module %s", module);
		goto c_p_d_error;
	}

	pFunc = PyObject_GetAttrString(pModule, method);
	if (pFunc == NULL)
	{
		my_error("PYDIRECT: The module %s has nothing named %s", module, method);
		goto c_p_d_error;
	}

	if (!PyCallable_Check(pFunc))
	{
		my_error("PYDIRECT: The thing named %s.%s is not a function", module, method);
		goto c_p_d_error;
	}

	if (!(pArgs = PyTuple_New(1)))
		goto c_p_d_error;

	if (!(args_py = Py_BuildValue("z", args)))
		goto c_p_d_error;

	if ((PyTuple_SetItem(pArgs, 0, args_py))) 
		goto c_p_d_error;
	args_py = NULL;			/* args_py now belongs to the tuple! */

	if (!(pRetVal = PyObject_CallObject(pFunc, pArgs)))
		goto c_p_d_error;

        if (!(retval_repr = PyObject_Repr(pRetVal)))
		goto c_p_d_error;

        if (!(r = PyUnicode_AsUTF8(retval_repr)))
		goto c_p_d_error;

        retvalstr = malloc_strdup(r);
	goto c_p_d_cleanup;

c_p_d_error:
	output_traceback();

c_p_d_cleanup:
	Py_XDECREF(pArgs);
	Py_XDECREF(pFunc);
	Py_XDECREF(pModule);
	Py_XDECREF(pRetVal);

	RETURN_MSTR(retvalstr);
}

BUILT_IN_COMMAND(pydirect_cmd)
{
	char *x;

	x = call_python_directly(args);
	new_free(&x);
}


void	output_traceback (void)
{
	PyObject *ptype, *pvalue, *ptraceback;
	PyObject *ptype_repr, *pvalue_repr, *ptraceback_repr;
	char *ptype_str, *pvalue_str, *ptraceback_str;

	say("The python evaluation failed. hrm.");
	PyErr_Fetch(&ptype, &pvalue, &ptraceback);
	if (ptype != NULL)
	{
		ptype_repr = PyObject_Repr(ptype);
		ptype_str = PyUnicode_AsUTF8(ptype_repr);
		say("Exception Type: %s", ptype_str);
	}
	if (pvalue != NULL)
	{
		pvalue_repr = PyObject_Repr(pvalue);
		pvalue_str = PyUnicode_AsUTF8(pvalue_repr);
		say("Value: %s", pvalue_str);
	}
	if (ptraceback != NULL)
	{
		ptraceback_repr = PyObject_Repr(ptraceback);
		ptraceback_str = PyUnicode_AsUTF8(ptraceback_repr);
		say("Traceback: %s", ptraceback_str);
	}
	Py_XDECREF(ptype);
	Py_XDECREF(pvalue);
	Py_XDECREF(ptraceback);
	return;
}

