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

void get_compile_info(ShaderLanguage::ShaderCompileInfo &info, RenderingServer::ShaderMode p_mode = RenderingServer::SHADER_SPATIAL) {
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

#define TEST_CONVERSION(old_code, expected, is_deprecated)       \
	{                                                            \
		ShaderDeprecatedConverter converter(old_code);           \
		CHECK_EQ(converter.is_code_deprecated(), is_deprecated); \
		CHECK_EQ(converter.convert_code(), true);                \
		String new_code = converter.emit_code();                 \
		CHECK_EQ(new_code, expected);                            \
	}

TEST_CASE("[ShaderDeprecatedConverter] Simple conversion with arrays") {
	String code = "shader_type particles; void vertex() { float xy[2] = {1.0,1.1}; }";
	String expected = "shader_type particles; void process() { float xy[2] = {1.0,1.1}; }";
	TEST_CONVERSION(code, expected, true);
}

TEST_CASE("[ShaderDeprecatedConverter] Simple conversion with arrays") {
	String code = "shader_type particles; struct foo{float bar;} void vertex() { float xy[2] = {1.0,1.1}; }";
	String expected = "shader_type particles; struct foo{float bar;} void process() { float xy[2] = {1.0,1.1}; }";
	TEST_CONVERSION(code, expected, true);
}

TEST_CASE("[ShaderDeprecatedConverter] new-style array declaration") {
	String code = "shader_type particles; void process() { float[2] xy = {1.0,1.1}; }";
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
	SUBCASE("with another function named `process`") {
		String code = "shader_type particles; void vertex() {}  void process() {}";
		String expected = "shader_type particles; void process() {}  void process_() {}";
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("with another function named `process` that is called") {
		String code = "shader_type particles; void process() {} void vertex() { process(); }";
		String expected = "shader_type particles; void process_() {} void process() { process_(); }";
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("with another function named `process` which calls `vertex`") {
		String code = "shader_type particles; void process() {vertex();} void vertex() {} void foo() { vertex(); }";
		String expected = "shader_type particles; void process_() {process();} void process() {} void foo() { process(); }";
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("No function named `vertex`") {
		String code = "shader_type particles; void process() {}";
		// Should be unchanged.
		TEST_CONVERSION(code, code, false);
	}
}

TEST_CASE("[ShaderDeprecatedConverter] CLEARCOAT_GLOSS -> CLEARCOAT_ROUGHNESS") {
	SUBCASE("left-hand simple assignment") {
		String code("shader_type spatial; void fragment() {\n"
					"CLEARCOAT_GLOSS = 1.0;\n"
					"}\n");
		String expected("shader_type spatial; void fragment() {\n"
						"CLEARCOAT_ROUGHNESS = (1.0 - (1.0));\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("left-hand *= assignment") {
		String code("shader_type spatial; void fragment() {\n"
					"CLEARCOAT_GLOSS *= 0.5;\n"
					"}\n");
		String expected("shader_type spatial; void fragment() {\n"
						"CLEARCOAT_ROUGHNESS = (1.0 - ((1.0 - CLEARCOAT_ROUGHNESS) * 0.5));\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("right-hand usage") {
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
	SUBCASE("without clobbering existing casts") {
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

TEST_CASE("[ShaderDeprecatedConverter] All built-in renames") {
	String code_template = "shader_type %s; void %s() { %s; }";
	// Get all the built-in renames.
	List<String> builtins;
	ShaderDeprecatedConverter::_get_builtin_renames_list(&builtins);
	Vector<RS::ShaderMode> modes = { RS::SHADER_SPATIAL, RS::SHADER_CANVAS_ITEM, RS::SHADER_PARTICLES };
	for (RS::ShaderMode mode : modes) {
		for (const String &builtin : builtins) {
			if (ShaderDeprecatedConverter::_rename_has_special_handling(builtin)) {
				continue; // skip
			}

			// Now get the funcs applicable for this mode and built-in.
			Vector<String> funcs = ShaderDeprecatedConverter::_get_funcs_builtin_rename(mode, builtin);
			String rename = ShaderDeprecatedConverter::get_builtin_rename(builtin);
			for (const String &func : funcs) {
				String code = vformat(code_template, get_shader_mode_name(mode), func, builtin);
				String expected = vformat(code_template, get_shader_mode_name(mode), func, rename);
				TEST_CONVERSION(code, expected, true);
			}
		}
	}
}

TEST_CASE("[ShaderDeprecatedConverter] No renaming built-ins in non-candidate functions") {
	String code_template = "shader_type %s; void %s() { float %s = 1.0; %s += 1.0; }";
	List<String> builtins;
	ShaderDeprecatedConverter::_get_builtin_renames_list(&builtins);
	Vector<RS::ShaderMode> modes = { RS::SHADER_SPATIAL, RS::SHADER_CANVAS_ITEM, RS::SHADER_PARTICLES };
	for (RS::ShaderMode mode : modes) {
		ShaderLanguage::ShaderCompileInfo info;
		get_compile_info(info, mode);
		for (const String &builtin : builtins) {
			if (ShaderDeprecatedConverter::_rename_has_special_handling(builtin)) {
				continue; // skip
			}
			String rename = ShaderDeprecatedConverter::get_builtin_rename(builtin);
			Vector<String> candidate_funcs = ShaderDeprecatedConverter::_get_funcs_builtin_rename(mode, builtin);
			Vector<String> non_funcs;
			for (KeyValue<StringName, ShaderLanguage::FunctionInfo> &func : info.functions) {
				if (func.key == "global") {
					continue;
				}
				if (!candidate_funcs.has(func.key)) {
					non_funcs.push_back(func.key);
				}
			}

			for (const String &func : non_funcs) {
				String code = vformat(code_template, get_shader_mode_name(mode), func, builtin, builtin);
				// The code should not change.
				TEST_CONVERSION(code, code, false);
			}
		}
	}
}

TEST_CASE("[ShaderDeprecatedConverter] No renaming built-ins in candidate functions with built-in declared") {
	// For example, "shader_type spatial; void fragment() { float NORMALMAP = 1.0; }" is valid 4.x code but not valid 3.x code
	String code_template = "shader_type %s; void %s() { float %s = 1.0; %s += 1.0; }";
	// Get all the built-in renames.
	List<String> builtins;
	ShaderDeprecatedConverter::_get_builtin_renames_list(&builtins);
	Vector<RS::ShaderMode> modes = { RS::SHADER_SPATIAL, RS::SHADER_CANVAS_ITEM, RS::SHADER_PARTICLES };
	for (RS::ShaderMode mode : modes) {
		for (const String &builtin : builtins) {
			if (ShaderDeprecatedConverter::_rename_has_special_handling(builtin)) {
				continue; // skip
			}
			Vector<String> funcs = ShaderDeprecatedConverter::_get_funcs_builtin_rename(mode, builtin);
			for (const String &func : funcs) {
				String code = vformat(code_template, get_shader_mode_name(mode), func, builtin, builtin);
				CHECK_EQ(code.is_empty(), false);
				// The code should not change.
				TEST_CONVERSION(code, code, false);
			}
		}
	}
}

// If this fails, remove the MODULATE entry from ShaderDeprecatedConverter::removed_builtins, then remove this test.
TEST_CASE("[ShaderDeprecatedConverter] MODULATE is not a built-in on canvas_item") {
	ShaderLanguage::ShaderCompileInfo info;
	get_compile_info(info, RS::ShaderMode::SHADER_CANVAS_ITEM);
	for (const String &func : Vector<String>{ "vertex", "fragment", "light" }) {
		auto &finfo = info.functions[func];
		CHECK_EQ(finfo.built_ins.has("MODULATE"), false);
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
			auto type = ShaderDeprecatedConverter::get_removed_builtin_type(builtin);
			auto hints = ShaderDeprecatedConverter::get_removed_builtin_hints(builtin);
			auto funcs = ShaderDeprecatedConverter::_get_funcs_builtin_removal(mode, builtin);
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
				if (type == ShaderDeprecatedConverter::TokenType::TK_ERROR) { // Unported builtins.
					ShaderDeprecatedConverter converter(code);
					CHECK_EQ(converter.is_code_deprecated(), true);
					CHECK_EQ(converter.convert_code(), false);
					converter.set_fail_on_unported(false);
					CHECK_EQ(converter.convert_code(), true);
					continue;
				}
				String expected = vformat(code_template, get_shader_mode_name(mode), uniform_decl, func, builtin);
				TEST_CONVERSION(code, expected, true);
			}
		}
	}
}

TEST_CASE("[ShaderDeprecatedConverter] Test replacement of reserved keywords") {
	Vector<String> keywords;
	for (int i = 0; i < ShaderLanguage::TK_MAX; i++) {
		if (ShaderDeprecatedConverter::tokentype_is_new_reserved_keyword(static_cast<ShaderLanguage::TokenType>(i))) {
			keywords.push_back(ShaderDeprecatedConverter::get_tokentype_text(static_cast<ShaderLanguage::TokenType>(i)));
		}
	}

	static const char *decl_test_template[]{
		"shader_type %s;\nvoid %s() {}\n",
		"shader_type %s;\nvoid test_func() {float %s;}\n",
		"shader_type %s;\nuniform sampler2D %s;\n",
		"shader_type %s;\nconst float %s = 1.0;\n",
		"shader_type %s;\nvarying float %s;\n",
		nullptr
	};
	Vector<String> shader_types_to_test = { "spatial", "canvas_item", "particles" };
	for (auto shader_type : shader_types_to_test) {
		ShaderLanguage::ShaderCompileInfo info;
		get_compile_info(info, get_shader_mode(shader_type));
		for (const String &keyword : keywords) {
			for (int i = 0; decl_test_template[i] != nullptr; i++) {
				if (shader_type == "particles" && String(decl_test_template[i]).contains("varying")) {
					continue;
				}
				String code = vformat(decl_test_template[i], shader_type, keyword);
				ShaderLanguage sl;
				CHECK_NE(sl.compile(code, info), Error::OK);
				sl.clear();
				String expected = vformat(decl_test_template[i], shader_type, keyword + "_");
				TEST_CONVERSION(code, expected, true);
			}
		}
	}
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
	for (const String &removed_type : removed_types) {
		for (int i = 0; decl_test_template[i] != nullptr; i++) {
			String code = vformat(decl_test_template[i], removed_type);
			ShaderDeprecatedConverter converter(code);
			CHECK_EQ(converter.is_code_deprecated(), true);
			CHECK_EQ(converter.convert_code(), false);
			converter.set_fail_on_unported(false);
			CHECK_EQ(converter.convert_code(), true);
		}
	}
}

} // namespace TestShaderConverter
#undef TEST_CONVERSION
#endif // DISABLE_DEPRECATED

#endif // TEST_SHADER_CONVERTER_H
