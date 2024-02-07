/**************************************************************************/
/*  shader_converter.h                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef SHADER_CONVERTER_H
#define SHADER_CONVERTER_H

#ifndef DISABLE_DEPRECATED

#include "core/templates/pair.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering_server.h"

class ShaderDeprecatedConverter {
public:
	using TokenType = ShaderLanguage::TokenType;
	using Token = ShaderLanguage::Token;
	using TT = TokenType;
	using TokE = List<Token>::Element;

	ShaderDeprecatedConverter() = delete;
	explicit ShaderDeprecatedConverter(const String &p_code);
	bool convert_code();
	bool is_code_deprecated();
	String get_error_text() const;
	int get_error_line() const;
	String emit_code() const;
	void set_add_comments(bool p_add_comments);
	void set_fail_on_unported(bool p_fail_on_unported);
	void set_assume_correct(bool p_assume_correct);

	static bool token_is_skippable(const Token &tk);
	static bool tokentype_is_new_reserved_keyword(const TokenType &tk_type);
	static bool tokentype_is_identifier(const TokenType &tk_type);
	static bool tokentype_is_new_type(const TokenType &tk_type);
	static bool token_is_type(const Token &tk);
	static bool token_is_hint(const Token &tk);
	static bool tokentype_is_new_hint(const TokenType &tk);
	static bool token_is_new_builtin_func(const Token &tk);

	static String get_tokentype_text(TokenType tk_type);

	static bool has_builtin_rename(RS::ShaderMode p_mode, const String &p_name, const String &p_function = "");
	static String get_builtin_rename(const String &p_name);

	static bool has_hint_replacement(const String &p_name);
	static TokenType get_hint_replacement(const String &p_name);

	static bool is_renamed_render_mode(RS::ShaderMode p_mode, const String &p_name);
	static String get_render_mode_rename(const String &p_name);

	static bool has_removed_render_mode(RS::ShaderMode p_mode, const String &p_name);
	static bool can_remove_render_mode(const String &p_name);

	static bool has_removed_type(const String &p_name);

	static bool is_renamed_function(RS::ShaderMode p_mode, const String &p_name);
	static String get_renamed_function(const String &p_name);
	static TokenType get_renamed_function_type(const String &p_name);

	static bool is_removed_builtin(RS::ShaderMode p_mode, const String &p_name, const String &p_function = "");
	static TokenType get_removed_builtin_type(const String &p_name);
	static Vector<TokenType> get_removed_builtin_hints(const String &p_name);

	static bool _rename_has_special_handling(const String &p_name);

	static void _get_builtin_renames_list(List<String> *r_list);
	static void _get_render_mode_renames_list(List<String> *r_list);
	static void _get_hint_renames_list(List<String> *r_list);
	static void _get_function_renames_list(List<String> *r_list);
	static void _get_render_mode_removals_list(List<String> *r_list);
	static void _get_builtin_removals_list(List<String> *r_list);
	static void _get_type_removals_list(List<String> *r_list);
	static Vector<String> _get_funcs_builtin_rename(RS::ShaderMode p_mode, const String &p_name);
	static Vector<String> _get_funcs_builtin_removal(RS::ShaderMode p_mode, const String &p_name);

	struct RenamedBuiltins {
		const char *name;
		const char *replacement;
		const Vector<Pair<RS::ShaderMode, Vector<String>>> mode_functions;
		const bool special_handling;
	};

	struct RenamedRenderModes {
		const RS::ShaderMode mode;
		const char *name;
		const char *replacement;
	};

	struct RenamedHints {
		const char *name;
		const ShaderLanguage::TokenType replacement;
	};

	struct RenamedFunctions {
		const RS::ShaderMode mode;
		const ShaderLanguage::TokenType type;
		const char *name;
		const char *replacement;
	};

	struct RemovedRenderModes {
		const RS::ShaderMode mode;
		const char *name;
		const bool can_remove;
	};

	struct RemovedBuiltins {
		const char *name;
		const ShaderLanguage::TokenType uniform_type;
		const Vector<ShaderLanguage::TokenType> hints;
		const Vector<Pair<RS::ShaderMode, Vector<String>>> mode_functions;
	};

private:
	struct UniformDecl {
		List<Token>::Element *start_pos;
		List<Token>::Element *end_pos;
		List<Token>::Element *type_pos;
		List<Token>::Element *name_pos;
		Vector<List<Token>::Element *> hint_poses;
		bool is_array = false;
	};
	struct VarDecl {
		List<Token>::Element *start_pos; // Varying token, const token, type token, or identifier if compound declaration (e.g. 'vec3 a, b;')
		List<Token>::Element *end_pos; // semicolon or comma or right paren
		List<Token>::Element *type_pos;
		List<Token>::Element *name_pos;
		bool is_array = false;
		bool new_arr_style_decl = false;
		bool is_func_arg = false;
		void clear() {
			start_pos = nullptr;
			end_pos = nullptr;
			type_pos = nullptr;
			name_pos = nullptr;
		}
	};

	struct FunctionDecl {
		List<Token>::Element *start_pos; // type or const
		List<Token>::Element *type_pos;
		List<Token>::Element *name_pos;
		List<Token>::Element *args_start_pos; // left paren
		List<Token>::Element *args_end_pos; // right paren
		List<Token>::Element *body_start_pos; // left curly
		List<Token>::Element *body_end_pos; // right curly - end of function
		bool has_array_return_type = false;
		void clear() {
			type_pos = nullptr;
			name_pos = nullptr;
			args_start_pos = nullptr;
			args_end_pos = nullptr;
			body_start_pos = nullptr;
			body_end_pos = nullptr;
		}
	};
	static const RenamedBuiltins renamed_builtins[];
	static const RenamedRenderModes renamed_render_modes[];
	static const RenamedHints renamed_hints[];
	static const RenamedFunctions renamed_functions[];
	static const RemovedRenderModes removed_render_modes[];
	static const RemovedBuiltins removed_builtins[];
	static const char *removed_types[];

	List<Token> code_tokens;
	List<Token>::Element *curr_ptr;
	List<Token>::Element *after_type_decl;
	HashMap<String, UniformDecl> uniform_decls;
	HashMap<String, Vector<VarDecl>> var_decls;
	HashMap<String, FunctionDecl> function_decls;
	HashMap<String, HashSet<String>> scope_declarations;
	RenderingServer::ShaderMode shader_mode;
	const String old_code;
	bool assume_correct = true;
	bool add_comments = false;
	bool fail_on_unported = true;

	bool function_pass_failed = false;
	bool var_pass_failed = false;
	String err_str;
	int err_line = 0;
	Token eof_token{ ShaderLanguage::TK_EOF, {}, 0, 0, 0, 0 };
	static RS::ShaderMode get_shader_mode_from_string(const String &p_mode);

	String get_token_literal_text(const Token &tk) const;
	static Token mkTok(TokenType p_type, const StringName &p_text = StringName(), double constant = 0, uint16_t p_line = 0);

	void reset();
	bool preprocess_code();
	List<Token>::Element *get_next_token();
	List<Token>::Element *get_prev_token();
	List<Token>::Element *remove_cur_and_get_next();
	TokenType peek_next_tk_type(uint32_t count = 1) const;
	TokenType peek_prev_tk_type(uint32_t count = 1) const;
	List<Token>::Element *get_pos() const;
	bool reset_to(List<Token>::Element *p_pos);
	bool insert_after(const Vector<Token> &token_list, List<Token>::Element *p_pos);
	bool insert_before(const Vector<Token> &token_list, List<Token>::Element *p_pos);
	bool insert_after(const Token &token, List<Token>::Element *p_pos);
	bool insert_before(const Token &token, List<Token>::Element *p_pos);
	List<Token>::Element *replace_curr(const Token &token);
	List<Token>::Element *_get_next_token_ptr(List<Token>::Element *_curr_ptr) const;
	List<Token>::Element *_get_prev_token_ptr(List<Token>::Element *_curr_ptr) const;
	TokenType _peek_tk_type(int64_t count, List<Token>::Element **r_pos = nullptr) const;

	bool scope_has_decl(const String &p_scope, const String &p_name) const;

	bool _skip_struct();
	bool _add_comment_before(const String &p_comment, List<Token>::Element *p_pos = nullptr);
	bool _add_comment_at_eol(const String &p_comment, List<Token>::Element *p_pos = nullptr);

	bool _insert_uniform_declaration(const String &p_name);
	List<Token>::Element *_remove_from_curr_to(List<Token>::Element *p_end);
	List<Token>::Element *_get_end_of_closure();

	enum {
		NEW_IDENT = -1
	};
};
#endif // DISABLE_DEPRECATED

#endif // SHADER_CONVERTER_H
