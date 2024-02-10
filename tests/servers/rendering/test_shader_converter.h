/**************************************************************************/
/*  test_shader_converter.h                                               */
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

#ifndef TEST_SHADER_CONVERTER_H
#define TEST_SHADER_CONVERTER_H

#ifndef DISABLE_DEPRECATED
#include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/shader_converter.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering/shader_types.h"
#include "tests/test_macros.h"

#include <cctype>

namespace TestShaderConverter {

void erase_all_empty(Vector<String> &p_vec) {
	int idx = p_vec.find(" ");
	while (idx >= 0) {
		p_vec.remove_at(idx);
		idx = p_vec.find(" ");
	}
}

bool is_variable_char(unsigned char c) {
	return std::isalnum(c) || c == '_';
}

bool is_operator_char(unsigned char c) {
	return (c == '*') || (c == '+') || (c == '-') || (c == '/') || ((c >= '<') && (c <= '>'));
}

// Remove unnecessary spaces from a line.
String remove_spaces(String &p_str) {
	String res;
	// Result is guaranteed to not be longer than the input.
	res.resize(p_str.size());
	int wp = 0;
	char32_t last = 0;
	bool has_removed = false;

	for (int n = 0; n < p_str.size(); n++) {
		// These test cases only use ASCII.
		unsigned char c = static_cast<unsigned char>(p_str[n]);
		if (std::isblank(c)) {
			has_removed = true;
		} else {
			if (has_removed) {
				// Insert a space to avoid joining things that could potentially form a new token.
				// E.g. "float x" or "- -".
				if ((is_variable_char(c) && is_variable_char(last)) ||
						(is_operator_char(c) && is_operator_char(last))) {
					res[wp++] = ' ';
				}
				has_removed = false;
			}
			res[wp++] = c;
			last = c;
		}
	}
	res.resize(wp);
	return res;
}

// The pre-processor changes indentation and inserts spaces when inserting macros.
// Re-format the code, without changing its meaning, to make it easier to compare.
String compact_spaces(String &p_str) {
	Vector<String> lines = p_str.split("\n", false);
	erase_all_empty(lines);
	for (String &line : lines) {
		line = remove_spaces(line);
	}
	return String("\n").join(lines);
}

#define CHECK_SHADER_EQ(a, b) CHECK_EQ(compact_spaces(a), compact_spaces(b))
#define CHECK_SHADER_NE(a, b) CHECK_NE(compact_spaces(a), compact_spaces(b))

void get_compile_info(ShaderLanguage::ShaderCompileInfo &info, RenderingServer::ShaderMode p_mode) {
	info.functions = ShaderTypes::get_singleton()->get_functions(p_mode);
	info.render_modes = ShaderTypes::get_singleton()->get_modes(p_mode);
	info.shader_types = ShaderTypes::get_singleton()->get_types();
	// Only used by editor for completion, so it's not important for these tests.
	info.global_shader_uniform_type_func = [](const StringName &p_name) -> ShaderLanguage::DataType {
		return ShaderLanguage::TYPE_SAMPLER2D;
	};
}

RenderingServer::ShaderMode get_shader_mode(const String &p_mode_string) {
	if (p_mode_string == "canvas_item") {
		return RS::SHADER_CANVAS_ITEM;
	} else if (p_mode_string == "particles") {
		return RS::SHADER_PARTICLES;
	} else if (p_mode_string == "spatial") {
		return RS::SHADER_SPATIAL;
	} else if (p_mode_string == "sky") {
		return RS::SHADER_SKY;
	} else if (p_mode_string == "fog") {
		return RS::SHADER_FOG;
	} else {
		return RS::SHADER_MAX;
	}
}

String get_shader_mode_name(const RenderingServer::ShaderMode &p_mode_string) {
	switch (p_mode_string) {
		case RS::SHADER_CANVAS_ITEM:
			return "canvas_item";
		case RS::SHADER_PARTICLES:
			return "particles";
		case RS::SHADER_SPATIAL:
			return "spatial";
		case RS::SHADER_SKY:
			return "sky";
		case RS::SHADER_FOG:
			return "fog";
		default:
			return "unknown";
	}
}
using SL = ShaderLanguage;
using SDC = ShaderDeprecatedConverter;

#define TEST_CONVERSION(m_old_code, m_expected, m_is_deprecated)                \
	{                                                                           \
		ShaderDeprecatedConverter _i_converter;                                 \
		CHECK_EQ(_i_converter.is_code_deprecated(m_old_code), m_is_deprecated); \
		CHECK(_i_converter.convert_code(m_old_code));                           \
		String _i_new_code = _i_converter.emit_code();                          \
		CHECK_EQ(_i_new_code, m_expected);                                      \
	}

TEST_CASE("[ShaderDeprecatedConverter] Simple conversion with arrays") {
	String code = "shader_type particles; void vertex() { float xy[2] = {1.0,1.1}; }";
	String expected = "shader_type particles; void process() { float xy[2] = {1.0,1.1}; }";
	TEST_CONVERSION(code, expected, true);
}

TEST_CASE("[ShaderDeprecatedConverter] Test removed types") {
	static const char *decl_test_template[]{
		"shader_type spatial;\n%s foo() {}\n",
		"shader_type spatial;\nvoid test_func() {%s foo;}\n",
		"shader_type spatial;\nvarying %s foo;\n",
		nullptr
	};
	Vector<String> shader_types_to_test = { "spatial", "canvas_item", "particles" };
	List<String> removed_types;
	ShaderDeprecatedConverter::_get_type_removals_list(&removed_types);
	if (removed_types.is_empty()) {
		WARN_PRINT("No removed types found, this test is not useful.");
		return;
	}
	ShaderLanguage::ShaderCompileInfo info;
	get_compile_info(info, RS::SHADER_SPATIAL);
	SUBCASE("Code with removed types fail to compile") {
		for (const String &removed_type : removed_types) {
			for (int i = 0; decl_test_template[i] != nullptr; i++) {
				String code = vformat(decl_test_template[i], removed_type);
				ShaderLanguage sl;
				CHECK_NE(sl.compile(code, info), Error::OK);
			}
		}
	}
	SUBCASE("Convert code with removed types: fail_on_unported=true") {
		for (const String &removed_type : removed_types) {
			for (int i = 0; decl_test_template[i] != nullptr; i++) {
				String code = vformat(decl_test_template[i], removed_type);
				ShaderDeprecatedConverter converter;
				CHECK(converter.is_code_deprecated(code));
				CHECK_FALSE(converter.convert_code(code));
			}
		}
	}
	SUBCASE("Convert code with removed types: fail_on_unported=false") {
		for (const String &removed_type : removed_types) {
			for (int i = 0; decl_test_template[i] != nullptr; i++) {
				String code = vformat(decl_test_template[i], removed_type);
				ShaderDeprecatedConverter converter;
				CHECK(converter.is_code_deprecated(code));
				converter.set_warning_comments(false);
				converter.set_fail_on_unported(false);
				CHECK(converter.convert_code(code));
				String new_code = converter.emit_code();
				CHECK_EQ(new_code, code);
				converter.set_warning_comments(true);
				new_code = converter.emit_code();
				CHECK_NE(new_code, code);
				CHECK(new_code.find("/* !convert WARNING:") != -1);
			}
		}
	}
}

TEST_CASE("[ShaderDeprecatedConverter] Test warning comments") {
	// Test that warning comments are added when fail_on_unported is false and warning_comments is true
	String code = "shader_type spatial;\nrender_mode specular_phong;";
	String expected = "shader_type spatial;\n/* !convert WARNING: Deprecated render mode 'specular_phong' is not supported by this version of Godot. */\nrender_mode specular_phong;";
	ShaderDeprecatedConverter converter;
	CHECK(converter.is_code_deprecated(code));
	converter.set_warning_comments(true);
	converter.set_fail_on_unported(false);
	CHECK(converter.convert_code(code));
	String new_code = converter.emit_code();
	CHECK_EQ(new_code, expected);
}

TEST_CASE("[ShaderDeprecatedConverter] Simple conversion with arrays") {
	String code = "shader_type particles; struct foo{float bar;} void vertex() { float xy[2] = {1.0,1.1}; }";
	String expected = "shader_type particles; struct foo{float bar;} void process() { float xy[2] = {1.0,1.1}; }";
	TEST_CONVERSION(code, expected, true);
}

TEST_CASE("[ShaderDeprecatedConverter] new-style array declaration") {
	String code = "shader_type spatial; void vertex() { float[2] xy = {1.0,1.1}; }";
	// code should be the same
	TEST_CONVERSION(code, code, false);
}

TEST_CASE("[ShaderDeprecatedConverter] Simple conversion") {
	String code = "shader_type particles; void vertex() { float x = 1.0; }";
	String expected = "shader_type particles; void process() { float x = 1.0; }";
	TEST_CONVERSION(code, expected, true);
}

TEST_CASE("[ShaderDeprecatedConverter] Replace non-conformant float literals") {
	String code = "shader_type spatial; const float x = 1f;";
	String expected = "shader_type spatial; const float x = 1.0f;";
	TEST_CONVERSION(code, expected, true);
}

TEST_CASE("[ShaderDeprecatedConverter] particles::vertex() -> particles::process()") {
	SUBCASE("basic") {
		String code = "shader_type particles; void vertex() { float x = 1.0; }";
		String expected = "shader_type particles; void process() { float x = 1.0; }";
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("with another function named `process` without correct signature") {
		String code = "shader_type particles; void vertex() {}  float process() { return 1.0; }";
		String expected = "shader_type particles; void process() {}  float process_() { return 1.0; }";
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("with another function named `process` with correct signature") {
		String code = "shader_type particles; void vertex() {}  void process() {}";
		// Should be unchanged.
		TEST_CONVERSION(code, code, false);
	}

	SUBCASE("with another function named `process` that is called") {
		String code = "shader_type particles; float process() { return 1.0; } void vertex() { float foo = process(); }";
		String expected = "shader_type particles; float process_() { return 1.0; } void process() { float foo = process_(); }";
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("with another function named `process` which calls `vertex`") {
		String code = "shader_type particles; float process() {foo(); return 1.0;} void vertex() {} void foo() { vertex(); }";
		String expected = "shader_type particles; float process_() {foo(); return 1.0;} void process() {} void foo() { process(); }";
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("No function named `vertex`") {
		String code = "shader_type particles; void process() {}";
		// Should be unchanged.
		TEST_CONVERSION(code, code, false);
	}
}

TEST_CASE("[ShaderDeprecatedConverter] CLEARCOAT_GLOSS -> CLEARCOAT_ROUGHNESS") {
	SUBCASE("Left-hand simple assignment") {
		String code("shader_type spatial; void fragment() {\n"
					"CLEARCOAT_GLOSS = 1.0;\n"
					"}\n");
		String expected("shader_type spatial; void fragment() {\n"
						"CLEARCOAT_ROUGHNESS = (1.0 - (1.0));\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("Left-hand *= assignment") {
		String code("shader_type spatial; void fragment() {\n"
					"CLEARCOAT_GLOSS *= 0.5;\n"
					"}\n");
		String expected("shader_type spatial; void fragment() {\n"
						"CLEARCOAT_ROUGHNESS = (1.0 - ((1.0 - CLEARCOAT_ROUGHNESS) * 0.5));\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("Right-hand usage") {
		String code("shader_type spatial; void fragment() {\n"
					"float foo = CLEARCOAT_GLOSS;\n"
					"}\n");
		String expected("shader_type spatial; void fragment() {\n"
						"float foo = (1.0 - CLEARCOAT_ROUGHNESS);\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("both usages") {
		String code("shader_type spatial; void fragment() {\n"
					"float foo = (CLEARCOAT_GLOSS *= 0.5);\n"
					"}\n");
		String expected("shader_type spatial; void fragment() {\n"
						"float foo = ((1.0 - (CLEARCOAT_ROUGHNESS = (1.0 - ((1.0 - CLEARCOAT_ROUGHNESS) * 0.5)))));\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
}
TEST_CASE("[ShaderDeprecatedConverter] Wrap INDEX in int()") {
	SUBCASE("basic") {
		String code("shader_type particles; void vertex() {\n"
					"float foo = INDEX/2;\n"
					"}\n");
		String expected("shader_type particles; void process() {\n"
						"float foo = int(INDEX)/2;\n"
						"}\n");

		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("Without clobbering existing casts") {
		String code("shader_type particles; void vertex() {\n"
					"float foo = int(INDEX/2) * int(INDEX) * 2 * float(INDEX);\n"
					"}\n");
		String expected("shader_type particles; void process() {\n"
						"float foo = int(int(INDEX)/2) * int(INDEX) * 2 * float(INDEX);\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
}

TEST_CASE("[ShaderDeprecatedConverter] All hint renames") {
	String code_template = "shader_type spatial; uniform sampler2D foo : %s;";
	// get all the hint renames
	List<String> hints;
	ShaderDeprecatedConverter::_get_hint_renames_list(&hints);
	for (const String &hint : hints) {
		ShaderDeprecatedConverter::TokenType type = ShaderDeprecatedConverter::get_hint_replacement(hint);
		String rename = ShaderDeprecatedConverter::get_tokentype_text(type);
		String code = vformat(code_template, hint);
		String expected = vformat(code_template, rename);
		TEST_CONVERSION(code, expected, true);
	}
}

TEST_CASE("[ShaderDeprecatedConverter] Built-in renames") {
	// Get all the built-in renames.
	List<String> builtins;
	ShaderDeprecatedConverter::_get_builtin_renames_list(&builtins);
	// remove built-ins that have special handling, we test those above
	for (List<String>::Element *E = builtins.front(); E; E = E->next()) {
		if (ShaderDeprecatedConverter::_rename_has_special_handling(E->get())) {
			List<String>::Element *prev = E->prev();
			builtins.erase(E);
			E = prev;
		}
	}
	Vector<RS::ShaderMode> modes = { RS::SHADER_SPATIAL, RS::SHADER_CANVAS_ITEM, RS::SHADER_PARTICLES };
	HashMap<RS::ShaderMode, HashMap<String, Vector<String>>> rename_func_map;
	for (RS::ShaderMode mode : modes) {
		rename_func_map[mode] = HashMap<String, Vector<String>>();
		for (const String &builtin : builtins) {
			rename_func_map[mode][builtin] = ShaderDeprecatedConverter::_get_funcs_builtin_rename(mode, builtin);
		}
	}
	SUBCASE("All built-in renames are replaced") {
		String code_template = "shader_type %s; void %s() { %s; }";
		for (RS::ShaderMode mode : modes) {
			for (const String &builtin : builtins) {
				// Now get the funcs applicable for this mode and built-in.
				String rename = ShaderDeprecatedConverter::get_builtin_rename(builtin);
				for (const String &func : rename_func_map[mode][builtin]) {
					String code = vformat(code_template, get_shader_mode_name(mode), func, builtin);
					String expected = vformat(code_template, get_shader_mode_name(mode), func, rename);
					TEST_CONVERSION(code, expected, true);
				}
			}
		}
	}
	SUBCASE("No renaming built-ins in non-candidate functions") {
		String code_template = "shader_type %s; void %s() { float %s = 1.0; %s += 1.0; }";
		for (RS::ShaderMode mode : modes) {
			ShaderLanguage::ShaderCompileInfo info;
			get_compile_info(info, mode);
			for (const String &builtin : builtins) {
				Vector<String> non_funcs;
				for (KeyValue<StringName, ShaderLanguage::FunctionInfo> &func : info.functions) {
					if (func.key == "global") {
						continue;
					}
					if (!rename_func_map[mode][builtin].has(func.key)) {
						non_funcs.push_back(func.key);
					}
				}
				String rename = ShaderDeprecatedConverter::get_builtin_rename(builtin);
				for (const String &func : non_funcs) {
					String code = vformat(code_template, get_shader_mode_name(mode), func, builtin, builtin);
					// The code should not change.
					TEST_CONVERSION(code, code, false);
				}
			}
		}
	}
	SUBCASE("No renaming built-ins in candidate functions with built-in declared") {
		String code_template = "shader_type %s; void %s() { float %s = 1.0; %s += 1.0; }";
		for (RS::ShaderMode mode : modes) {
			for (const String &builtin : builtins) {
				for (const String &func : rename_func_map[mode][builtin]) {
					String code = vformat(code_template, get_shader_mode_name(mode), func, builtin, builtin);
					// The code should not change.
					TEST_CONVERSION(code, code, false);
				}
			}
		}
	}
}

// TODO: Remove this when the MODULATE built-in PR lands.
// If this fails, remove the MODULATE entry from ShaderDeprecatedConverter::removed_builtins, then remove this test and the following test.
TEST_CASE("[ShaderDeprecatedConverter] MODULATE is not a built-in") {
	ShaderLanguage::ShaderCompileInfo info;
	get_compile_info(info, RS::ShaderMode::SHADER_CANVAS_ITEM);
	SUBCASE("MODULATE is not a built-in") {
		for (const String &func : Vector<String>{ "vertex", "fragment", "light" }) {
			auto &finfo = info.functions[func];
			CHECK_FALSE(finfo.built_ins.has("MODULATE"));
		}
	}
}

// Don't remove this one if the above doesn't fail too.
TEST_CASE("[ShaderDeprecatedConverter] MODULATE handling") {
	ShaderLanguage::ShaderCompileInfo info;
	get_compile_info(info, RS::ShaderMode::SHADER_CANVAS_ITEM);
	SUBCASE("Fails to compile") {
		for (const String &func : Vector<String>{ "vertex", "fragment", "light" }) {
			String code = vformat("shader_type canvas_item; void %s() { MODULATE; }", func);
			ShaderLanguage sl;
			CHECK_NE(sl.compile(code, info), Error::OK);
		}
	}
	SUBCASE("Fails to convert on fail_on_unported=true") {
		for (const String &func : Vector<String>{ "vertex", "fragment", "light" }) {
			String code = vformat("shader_type canvas_item; void %s() { MODULATE; }", func);
			ShaderDeprecatedConverter converter;
			CHECK(converter.is_code_deprecated(code));
			converter.set_fail_on_unported(true);
			CHECK_FALSE(converter.convert_code(code));
		}
	}

	SUBCASE("Conversion succeeds on fail_on_unported=false") {
		for (const String &func : Vector<String>{ "vertex", "fragment", "light" }) {
			String code = vformat("shader_type canvas_item; void %s() { MODULATE; }", func);
			ShaderDeprecatedConverter converter;
			CHECK(converter.is_code_deprecated(code));
			converter.set_fail_on_unported(false);
			CHECK(converter.convert_code(code));
			String new_code = converter.emit_code();
			CHECK(new_code.find("/*") != -1);
		}
	}
}

TEST_CASE("[ShaderDeprecatedConverter] Uniform declarations for removed builtins") {
	// Test uniform declaration inserts for removed builtins for all shader types.
	String code_template = "shader_type %s;%s void %s() { %s; }";
	String uniform_template = "\nuniform %s %s : %s;\n";
	// Get all the removed built-ins.
	List<String> builtins;
	ShaderDeprecatedConverter::_get_builtin_removals_list(&builtins);
	Vector<RS::ShaderMode> modes = { RS::SHADER_SPATIAL, RS::SHADER_CANVAS_ITEM, RS::SHADER_PARTICLES };
	for (RS::ShaderMode mode : modes) {
		ShaderLanguage::ShaderCompileInfo info;
		get_compile_info(info, mode);
		for (const String &builtin : builtins) {
			// now get the funcs applicable for this mode and builtins
			ShaderLanguage::TokenType type = ShaderDeprecatedConverter::get_removed_builtin_uniform_type(builtin);
			if (type == ShaderDeprecatedConverter::TokenType::TK_ERROR) {
				continue;
			}
			Vector<ShaderLanguage::TokenType> hints = ShaderDeprecatedConverter::get_removed_builtin_hints(builtin);
			Vector<String> funcs = ShaderDeprecatedConverter::_get_funcs_builtin_removal(mode, builtin);
			String hint_string = "";
			for (int i = 0; i < hints.size(); i++) {
				hint_string += ShaderDeprecatedConverter::get_tokentype_text(hints[i]);
				if (i < hints.size() - 1) {
					hint_string += ", ";
				}
			}
			String uniform_decl = vformat(uniform_template, ShaderDeprecatedConverter::get_tokentype_text(type), builtin, hint_string);
			for (const String &func : funcs) {
				String code = vformat(code_template, get_shader_mode_name(mode), "", func, builtin);
				if (type == ShaderDeprecatedConverter::TokenType::TK_ERROR) { // Unported builtins with no uniform declaration
					ShaderDeprecatedConverter converter;
					CHECK(converter.is_code_deprecated(code));
					CHECK_FALSE(converter.convert_code(code));
					converter.set_fail_on_unported(false);
					CHECK(converter.convert_code(code));
					continue;
				}
				String expected = vformat(code_template, get_shader_mode_name(mode), uniform_decl, func, builtin);
				TEST_CONVERSION(code, expected, true);
			}
		}
	}
}

// Reserved keywords (i.e. non-built-in function keywords that have a discrete token type)
TEST_CASE("[ShaderDeprecatedConverter] Replacement of reserved keywords used as identifiers") {
	Vector<String> keywords;
	for (int i = 0; i < ShaderLanguage::TK_MAX; i++) {
		if (ShaderDeprecatedConverter::tokentype_is_new_reserved_keyword(static_cast<ShaderLanguage::TokenType>(i))) {
			keywords.push_back(ShaderDeprecatedConverter::get_tokentype_text(static_cast<ShaderLanguage::TokenType>(i)));
		}
	}
	Vector<String> hint_keywords;
	for (int i = 0; i < SL::TK_MAX; i++) {
		if (SDC::tokentype_is_new_hint(static_cast<SL::TokenType>(i))) {
			hint_keywords.push_back(SDC::get_tokentype_text(static_cast<SL::TokenType>(i)));
		}
	}
	Vector<String> uniform_quals;
	for (int i = 0; i < SL::TK_MAX; i++) {
		if (SL::is_token_uniform_qual(static_cast<SL::TokenType>(i))) {
			uniform_quals.push_back(SDC::get_tokentype_text(static_cast<SL::TokenType>(i)));
		}
	}
	Vector<String> shader_types_to_test = { "spatial", "canvas_item", "particles" };

	static const char *decl_test_template[]{
		"shader_type %s;\nvoid %s() {}\n",
		"shader_type %s;\nvoid test_func() {float %s;}\n",
		"shader_type %s;\nuniform sampler2D %s;\n",
		"shader_type %s;\nconst float %s = 1.0;\n",
		"shader_type %s;\nvarying float %s;\n",
		nullptr
	};
	// NOTE: if this fails, the current behavior of the converter to replace these has to be changed.
	SUBCASE("Code with reserved keywords used as identifiers fail to compile") {
		ShaderLanguage::ShaderCompileInfo info;
		get_compile_info(info, RS::SHADER_SPATIAL);
		for (const String &shader_type : shader_types_to_test) {
			for (const String &keyword : keywords) {
				for (int i = 0; decl_test_template[i] != nullptr; i++) {
					String code = vformat(decl_test_template[i], shader_type, keyword);
					ShaderLanguage sl;
					CHECK_NE(sl.compile(code, info), Error::OK);
				}
			}
		}
	}
	SUBCASE("Code with reserved keywords used as identifiers is converted successfully") {
		for (const String &shader_type : shader_types_to_test) {
			ShaderLanguage::ShaderCompileInfo info;
			get_compile_info(info, get_shader_mode(shader_type));
			for (const String &keyword : keywords) {
				for (int i = 0; decl_test_template[i] != nullptr; i++) {
					if (shader_type == "particles" && String(decl_test_template[i]).contains("varying")) {
						continue;
					}
					String code = vformat(decl_test_template[i], shader_type, keyword);
					String expected = vformat(decl_test_template[i], shader_type, keyword + "_");
					TEST_CONVERSION(code, expected, true);
				}
			}
		}
	}
	static const char *new_hint_test = "shader_type spatial;\nuniform sampler2D foo : %s; const float %s = 1.0;\n";
	SUBCASE("New hints used as hints are not replaced") {
		for (const String &hint : hint_keywords) {
			String code = vformat(new_hint_test, hint, "bar");
			// Code should not change.
			TEST_CONVERSION(code, code, false);
		}
	}

	SUBCASE("Mixed new hints used as hints and new hints used as identifiers") {
		for (const String &hint : hint_keywords) {
			String code = vformat(new_hint_test, hint, hint);
			// Should not change.
			ShaderDeprecatedConverter converter;
			CHECK_FALSE(converter.is_code_deprecated(code)); // Should be detected as not deprecated.
			converter.set_warning_comments(false);
			CHECK(converter.convert_code(code));
			String new_code = converter.emit_code();
			// Code should not change
			CHECK_EQ(new_code, code);
			// Check for warning comment
			converter.set_warning_comments(true);
			new_code = converter.emit_code();
			CHECK(new_code.contains("/* !convert WARNING:"));
		}
	}
	static const char *non_id_keyword_test = "shader_type spatial;\n%s uniform sampler2D foo; const float %s = 1.0;\n";
	SUBCASE("New keywords not used as identifiers are not replaced") {
		for (const String &qual : uniform_quals) {
			// e.g. "shader_type spatial;\nglobal uniform sampler2D foo; const float bar = 1.0;\n"
			String code = vformat(non_id_keyword_test, qual, "bar");
			// Code should not change.
			TEST_CONVERSION(code, code, false);
		}
	}

	SUBCASE("Mixed idiomatic new reserved words and new reserved words used as identifiers") {
		for (const String &qual : uniform_quals) {
			// e.g. "shader_type spatial;\nglobal uniform sampler2D foo; const float global = 1.0;\n"
			String code = vformat(non_id_keyword_test, qual, qual);
			// Should not change.
			ShaderDeprecatedConverter converter;
			CHECK_FALSE(converter.is_code_deprecated(code)); // Should be detected as not deprecated.
			converter.set_warning_comments(false);
			CHECK(converter.convert_code(code));
			String new_code = converter.emit_code();
			// Code should not change
			CHECK_EQ(new_code, code);
			// Check for warning comment
			converter.set_warning_comments(true);
			new_code = converter.emit_code();
			CHECK(new_code.contains("/* !convert WARNING:"));
		}
	}
}

} // namespace TestShaderConverter
#undef TEST_CONVERSION
#endif // DISABLE_DEPRECATED

#endif // TEST_SHADER_CONVERTER_H
