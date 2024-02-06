/**************************************************************************/
/*  shader_compiler.cpp                                                   */
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

#include "shader_compiler.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/shader_types.h"

#define SL ShaderLanguage

static String _mktab(int p_level) {
	return String("\t").repeat(p_level);
}

static String _typestr(SL::DataType p_type) {
	String type = ShaderLanguage::get_datatype_name(p_type);
	if (!RS::get_singleton()->is_low_end() && ShaderLanguage::is_sampler_type(p_type)) {
		type = type.replace("sampler", "texture"); //we use textures instead of samplers in Vulkan GLSL
	}
	return type;
}

static int _get_datatype_alignment(SL::DataType p_type) {
	switch (p_type) {
		case SL::TYPE_VOID:
			return 0;
		case SL::TYPE_BOOL:
			return 4;
		case SL::TYPE_BVEC2:
			return 8;
		case SL::TYPE_BVEC3:
			return 16;
		case SL::TYPE_BVEC4:
			return 16;
		case SL::TYPE_INT:
			return 4;
		case SL::TYPE_IVEC2:
			return 8;
		case SL::TYPE_IVEC3:
			return 16;
		case SL::TYPE_IVEC4:
			return 16;
		case SL::TYPE_UINT:
			return 4;
		case SL::TYPE_UVEC2:
			return 8;
		case SL::TYPE_UVEC3:
			return 16;
		case SL::TYPE_UVEC4:
			return 16;
		case SL::TYPE_FLOAT:
			return 4;
		case SL::TYPE_VEC2:
			return 8;
		case SL::TYPE_VEC3:
			return 16;
		case SL::TYPE_VEC4:
			return 16;
		case SL::TYPE_MAT2:
			return 16;
		case SL::TYPE_MAT3:
			return 16;
		case SL::TYPE_MAT4:
			return 16;
		case SL::TYPE_SAMPLER2D:
			return 16;
		case SL::TYPE_ISAMPLER2D:
			return 16;
		case SL::TYPE_USAMPLER2D:
			return 16;
		case SL::TYPE_SAMPLER2DARRAY:
			return 16;
		case SL::TYPE_ISAMPLER2DARRAY:
			return 16;
		case SL::TYPE_USAMPLER2DARRAY:
			return 16;
		case SL::TYPE_SAMPLER3D:
			return 16;
		case SL::TYPE_ISAMPLER3D:
			return 16;
		case SL::TYPE_USAMPLER3D:
			return 16;
		case SL::TYPE_SAMPLERCUBE:
			return 16;
		case SL::TYPE_SAMPLERCUBEARRAY:
			return 16;
		case SL::TYPE_STRUCT:
			return 0;
		case SL::TYPE_MAX: {
			ERR_FAIL_V(0);
		}
	}

	ERR_FAIL_V(0);
}

static String _interpstr(SL::DataInterpolation p_interp) {
	switch (p_interp) {
		case SL::INTERPOLATION_FLAT:
			return "flat ";
		case SL::INTERPOLATION_SMOOTH:
			return "";
		case SL::INTERPOLATION_DEFAULT:
			return "";
	}
	return "";
}

static String _prestr(SL::DataPrecision p_pres, bool p_force_highp = false) {
	switch (p_pres) {
		case SL::PRECISION_LOWP:
			return "lowp ";
		case SL::PRECISION_MEDIUMP:
			return "mediump ";
		case SL::PRECISION_HIGHP:
			return "highp ";
		case SL::PRECISION_DEFAULT:
			return p_force_highp ? "highp " : "";
	}
	return "";
}

static String _constr(bool p_is_const) {
	if (p_is_const) {
		return "const ";
	}
	return "";
}

static String _qualstr(SL::ArgumentQualifier p_qual) {
	switch (p_qual) {
		case SL::ARGUMENT_QUALIFIER_IN:
			return "";
		case SL::ARGUMENT_QUALIFIER_OUT:
			return "out ";
		case SL::ARGUMENT_QUALIFIER_INOUT:
			return "inout ";
	}
	return "";
}

static String _opstr(SL::Operator p_op) {
	return SL::get_operator_text(p_op);
}

static String _mkid(const String &p_id) {
	String id = "m_" + p_id.replace("__", "_dus_");
	return id.replace("__", "_dus_"); //doubleunderscore is reserved in glsl
}

static String f2sp0(float p_float) {
	String num = rtos(p_float);
	if (!num.contains(".") && !num.contains("e")) {
		num += ".0";
	}
	return num;
}

static String get_constant_text(SL::DataType p_type, const Vector<SL::ConstantNode::Value> &p_values) {
	switch (p_type) {
		case SL::TYPE_BOOL:
			return p_values[0].boolean ? "true" : "false";
		case SL::TYPE_BVEC2:
		case SL::TYPE_BVEC3:
		case SL::TYPE_BVEC4: {
			String text = "bvec" + itos(p_type - SL::TYPE_BOOL + 1) + "(";
			for (int i = 0; i < p_values.size(); i++) {
				if (i > 0) {
					text += ",";
				}

				text += p_values[i].boolean ? "true" : "false";
			}
			text += ")";
			return text;
		}

		case SL::TYPE_INT:
			return itos(p_values[0].sint);
		case SL::TYPE_IVEC2:
		case SL::TYPE_IVEC3:
		case SL::TYPE_IVEC4: {
			String text = "ivec" + itos(p_type - SL::TYPE_INT + 1) + "(";
			for (int i = 0; i < p_values.size(); i++) {
				if (i > 0) {
					text += ",";
				}

				text += itos(p_values[i].sint);
			}
			text += ")";
			return text;

		} break;
		case SL::TYPE_UINT:
			return itos(p_values[0].uint) + "u";
		case SL::TYPE_UVEC2:
		case SL::TYPE_UVEC3:
		case SL::TYPE_UVEC4: {
			String text = "uvec" + itos(p_type - SL::TYPE_UINT + 1) + "(";
			for (int i = 0; i < p_values.size(); i++) {
				if (i > 0) {
					text += ",";
				}

				text += itos(p_values[i].uint) + "u";
			}
			text += ")";
			return text;
		} break;
		case SL::TYPE_FLOAT:
			return f2sp0(p_values[0].real);
		case SL::TYPE_VEC2:
		case SL::TYPE_VEC3:
		case SL::TYPE_VEC4: {
			String text = "vec" + itos(p_type - SL::TYPE_FLOAT + 1) + "(";
			for (int i = 0; i < p_values.size(); i++) {
				if (i > 0) {
					text += ",";
				}

				text += f2sp0(p_values[i].real);
			}
			text += ")";
			return text;

		} break;
		case SL::TYPE_MAT2:
		case SL::TYPE_MAT3:
		case SL::TYPE_MAT4: {
			String text = "mat" + itos(p_type - SL::TYPE_MAT2 + 2) + "(";
			for (int i = 0; i < p_values.size(); i++) {
				if (i > 0) {
					text += ",";
				}

				text += f2sp0(p_values[i].real);
			}
			text += ")";
			return text;

		} break;
		default:
			ERR_FAIL_V(String());
	}
}

String ShaderCompiler::_get_sampler_name(ShaderLanguage::TextureFilter p_filter, ShaderLanguage::TextureRepeat p_repeat) {
	if (p_filter == ShaderLanguage::FILTER_DEFAULT) {
		ERR_FAIL_COND_V(actions.default_filter == ShaderLanguage::FILTER_DEFAULT, String());
		p_filter = actions.default_filter;
	}
	if (p_repeat == ShaderLanguage::REPEAT_DEFAULT) {
		ERR_FAIL_COND_V(actions.default_repeat == ShaderLanguage::REPEAT_DEFAULT, String());
		p_repeat = actions.default_repeat;
	}
	constexpr const char *name_mapping[] = {
		"SAMPLER_NEAREST_CLAMP",
		"SAMPLER_LINEAR_CLAMP",
		"SAMPLER_NEAREST_WITH_MIPMAPS_CLAMP",
		"SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP",
		"SAMPLER_NEAREST_WITH_MIPMAPS_ANISOTROPIC_CLAMP",
		"SAMPLER_LINEAR_WITH_MIPMAPS_ANISOTROPIC_CLAMP",
		"SAMPLER_NEAREST_REPEAT",
		"SAMPLER_LINEAR_REPEAT",
		"SAMPLER_NEAREST_WITH_MIPMAPS_REPEAT",
		"SAMPLER_LINEAR_WITH_MIPMAPS_REPEAT",
		"SAMPLER_NEAREST_WITH_MIPMAPS_ANISOTROPIC_REPEAT",
		"SAMPLER_LINEAR_WITH_MIPMAPS_ANISOTROPIC_REPEAT"
	};
	return String(name_mapping[p_filter + (p_repeat == ShaderLanguage::REPEAT_ENABLE ? ShaderLanguage::FILTER_DEFAULT : 0)]);
}

void ShaderCompiler::_dump_function_deps(const SL::ShaderNode *p_node, const StringName &p_for_func, const HashMap<StringName, String> &p_func_code, String &r_to_add, HashSet<StringName> &added) {
	int fidx = -1;

	for (int i = 0; i < p_node->vfunctions.size(); i++) {
		if (p_node->vfunctions[i].name == p_for_func) {
			fidx = i;
			break;
		}
	}

	ERR_FAIL_COND(fidx == -1);

	Vector<StringName> uses_functions;

	for (const StringName &E : p_node->vfunctions[fidx].uses_function) {
		uses_functions.push_back(E);
	}
	uses_functions.sort_custom<StringName::AlphCompare>(); //ensure order is deterministic so the same shader is always produced

	for (int k = 0; k < uses_functions.size(); k++) {
		if (added.has(uses_functions[k])) {
			continue; //was added already
		}

		_dump_function_deps(p_node, uses_functions[k], p_func_code, r_to_add, added);

		SL::FunctionNode *fnode = nullptr;

		for (int i = 0; i < p_node->vfunctions.size(); i++) {
			if (p_node->vfunctions[i].name == uses_functions[k]) {
				fnode = p_node->vfunctions[i].function;
				break;
			}
		}

		ERR_FAIL_NULL(fnode);

		r_to_add += "\n";

		String header;
		if (fnode->return_type == SL::TYPE_STRUCT) {
			header = _mkid(fnode->return_struct_name);
		} else {
			header = _typestr(fnode->return_type);
		}

		if (fnode->return_array_size > 0) {
			header += "[";
			header += itos(fnode->return_array_size);
			header += "]";
		}

		header += " ";
		header += _mkid(fnode->name);
		header += "(";

		for (int i = 0; i < fnode->arguments.size(); i++) {
			if (i > 0) {
				header += ", ";
			}
			header += _constr(fnode->arguments[i].is_const);
			if (fnode->arguments[i].type == SL::TYPE_STRUCT) {
				header += _qualstr(fnode->arguments[i].qualifier) + _mkid(fnode->arguments[i].type_str) + " " + _mkid(fnode->arguments[i].name);
			} else {
				header += _qualstr(fnode->arguments[i].qualifier) + _prestr(fnode->arguments[i].precision) + _typestr(fnode->arguments[i].type) + " " + _mkid(fnode->arguments[i].name);
			}
			if (fnode->arguments[i].array_size > 0) {
				header += "[";
				header += itos(fnode->arguments[i].array_size);
				header += "]";
			}
		}

		header += ")\n";
		r_to_add += header;
		r_to_add += p_func_code[uses_functions[k]];

		added.insert(uses_functions[k]);
	}
}

static String _get_global_shader_uniform_from_type_and_index(const String &p_buffer, const String &p_index, ShaderLanguage::DataType p_type) {
	switch (p_type) {
		case ShaderLanguage::TYPE_BOOL: {
			return "bool(floatBitsToUint(" + p_buffer + "[" + p_index + "].x))";
		}
		case ShaderLanguage::TYPE_BVEC2: {
			return "bvec2(floatBitsToUint(" + p_buffer + "[" + p_index + "].xy))";
		}
		case ShaderLanguage::TYPE_BVEC3: {
			return "bvec3(floatBitsToUint(" + p_buffer + "[" + p_index + "].xyz))";
		}
		case ShaderLanguage::TYPE_BVEC4: {
			return "bvec4(floatBitsToUint(" + p_buffer + "[" + p_index + "].xyzw))";
		}
		case ShaderLanguage::TYPE_INT: {
			return "floatBitsToInt(" + p_buffer + "[" + p_index + "].x)";
		}
		case ShaderLanguage::TYPE_IVEC2: {
			return "floatBitsToInt(" + p_buffer + "[" + p_index + "].xy)";
		}
		case ShaderLanguage::TYPE_IVEC3: {
			return "floatBitsToInt(" + p_buffer + "[" + p_index + "].xyz)";
		}
		case ShaderLanguage::TYPE_IVEC4: {
			return "floatBitsToInt(" + p_buffer + "[" + p_index + "].xyzw)";
		}
		case ShaderLanguage::TYPE_UINT: {
			return "floatBitsToUint(" + p_buffer + "[" + p_index + "].x)";
		}
		case ShaderLanguage::TYPE_UVEC2: {
			return "floatBitsToUint(" + p_buffer + "[" + p_index + "].xy)";
		}
		case ShaderLanguage::TYPE_UVEC3: {
			return "floatBitsToUint(" + p_buffer + "[" + p_index + "].xyz)";
		}
		case ShaderLanguage::TYPE_UVEC4: {
			return "floatBitsToUint(" + p_buffer + "[" + p_index + "].xyzw)";
		}
		case ShaderLanguage::TYPE_FLOAT: {
			return "(" + p_buffer + "[" + p_index + "].x)";
		}
		case ShaderLanguage::TYPE_VEC2: {
			return "(" + p_buffer + "[" + p_index + "].xy)";
		}
		case ShaderLanguage::TYPE_VEC3: {
			return "(" + p_buffer + "[" + p_index + "].xyz)";
		}
		case ShaderLanguage::TYPE_VEC4: {
			return "(" + p_buffer + "[" + p_index + "].xyzw)";
		}
		case ShaderLanguage::TYPE_MAT2: {
			return "mat2(" + p_buffer + "[" + p_index + "].xy," + p_buffer + "[" + p_index + "+1u].xy)";
		}
		case ShaderLanguage::TYPE_MAT3: {
			return "mat3(" + p_buffer + "[" + p_index + "].xyz," + p_buffer + "[" + p_index + "+1u].xyz," + p_buffer + "[" + p_index + "+2u].xyz)";
		}
		case ShaderLanguage::TYPE_MAT4: {
			return "mat4(" + p_buffer + "[" + p_index + "].xyzw," + p_buffer + "[" + p_index + "+1u].xyzw," + p_buffer + "[" + p_index + "+2u].xyzw," + p_buffer + "[" + p_index + "+3u].xyzw)";
		}
		default: {
			ERR_FAIL_V("void");
		}
	}
}

String ShaderCompiler::_dump_node_code(const SL::Node *p_node, int p_level, GeneratedCode &r_gen_code, IdentifierActions &p_actions, const DefaultIdentifierActions &p_default_actions, bool p_assigning, bool p_use_scope) {
	String code;

	switch (p_node->type) {
		case SL::Node::NODE_TYPE_SHADER: {
			SL::ShaderNode *pnode = (SL::ShaderNode *)p_node;

			for (int i = 0; i < pnode->render_modes.size(); i++) {
				if (p_default_actions.render_mode_defines.has(pnode->render_modes[i]) && !used_rmode_defines.has(pnode->render_modes[i])) {
					r_gen_code.defines.push_back(p_default_actions.render_mode_defines[pnode->render_modes[i]]);
					used_rmode_defines.insert(pnode->render_modes[i]);
				}

				if (p_actions.render_mode_flags.has(pnode->render_modes[i])) {
					*p_actions.render_mode_flags[pnode->render_modes[i]] = true;
				}

				if (p_actions.render_mode_values.has(pnode->render_modes[i])) {
					Pair<int *, int> &p = p_actions.render_mode_values[pnode->render_modes[i]];
					*p.first = p.second;
				}
			}

			// structs

			for (int i = 0; i < pnode->vstructs.size(); i++) {
				SL::StructNode *st = pnode->vstructs[i].shader_struct;
				String struct_code;

				struct_code += "struct ";
				struct_code += _mkid(pnode->vstructs[i].name);
				struct_code += " ";
				struct_code += "{\n";
				for (int j = 0; j < st->members.size(); j++) {
					SL::MemberNode *m = st->members[j];
					if (m->datatype == SL::TYPE_STRUCT) {
						struct_code += _mkid(m->struct_name);
					} else {
						struct_code += _prestr(m->precision);
						struct_code += _typestr(m->datatype);
					}
					struct_code += " ";
					struct_code += m->name;
					if (m->array_size > 0) {
						struct_code += "[";
						struct_code += itos(m->array_size);
						struct_code += "]";
					}
					struct_code += ";\n";
				}
				struct_code += "}";
				struct_code += ";\n";

				for (int j = 0; j < STAGE_MAX; j++) {
					r_gen_code.stage_globals[j] += struct_code;
				}
			}

			int max_texture_uniforms = 0;
			int max_uniforms = 0;

			for (const KeyValue<StringName, SL::ShaderNode::Uniform> &E : pnode->uniforms) {
				if (SL::is_sampler_type(E.value.type)) {
					if (E.value.hint == SL::ShaderNode::Uniform::HINT_SCREEN_TEXTURE ||
							E.value.hint == SL::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE ||
							E.value.hint == SL::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
						continue; // Don't create uniforms in the generated code for these.
					}
					max_texture_uniforms++;
				} else {
					if (E.value.scope == SL::ShaderNode::Uniform::SCOPE_INSTANCE) {
						continue; // Instances are indexed directly, don't need index uniforms.
					}

					max_uniforms++;
				}
			}

			r_gen_code.texture_uniforms.resize(max_texture_uniforms);

			Vector<int> uniform_sizes;
			Vector<int> uniform_alignments;
			Vector<StringName> uniform_defines;
			uniform_sizes.resize(max_uniforms);
			uniform_alignments.resize(max_uniforms);
			uniform_defines.resize(max_uniforms);
			bool uses_uniforms = false;

			Vector<StringName> uniform_names;

			for (const KeyValue<StringName, SL::ShaderNode::Uniform> &E : pnode->uniforms) {
				uniform_names.push_back(E.key);
			}

			uniform_names.sort_custom<StringName::AlphCompare>(); //ensure order is deterministic so the same shader is always produced

			for (int k = 0; k < uniform_names.size(); k++) {
				const StringName &uniform_name = uniform_names[k];
				const SL::ShaderNode::Uniform &uniform = pnode->uniforms[uniform_name];

				String ucode;

				if (uniform.scope == SL::ShaderNode::Uniform::SCOPE_INSTANCE) {
					//insert, but don't generate any code.
					p_actions.uniforms->insert(uniform_name, uniform);
					continue; // Instances are indexed directly, don't need index uniforms.
				}

				if (uniform.hint == SL::ShaderNode::Uniform::HINT_SCREEN_TEXTURE ||
						uniform.hint == SL::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE ||
						uniform.hint == SL::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
					continue; // Don't create uniforms in the generated code for these.
				}

				if (SL::is_sampler_type(uniform.type)) {
					// Texture layouts are different for OpenGL GLSL and Vulkan GLSL
					if (!RS::get_singleton()->is_low_end()) {
						ucode = "layout(set = " + itos(actions.texture_layout_set) + ", binding = " + itos(actions.base_texture_binding_index + uniform.texture_binding) + ") ";
					}
					ucode += "uniform ";
				}

				bool is_buffer_global = !SL::is_sampler_type(uniform.type) && uniform.scope == SL::ShaderNode::Uniform::SCOPE_GLOBAL;

				if (is_buffer_global) {
					//this is an integer to index the global table
					ucode += _typestr(ShaderLanguage::TYPE_UINT);
				} else {
					ucode += _prestr(uniform.precision, ShaderLanguage::is_float_type(uniform.type));
					ucode += _typestr(uniform.type);
				}

				ucode += " " + _mkid(uniform_name);
				if (uniform.array_size > 0) {
					ucode += "[";
					ucode += itos(uniform.array_size);
					ucode += "]";
				}
				ucode += ";\n";
				if (SL::is_sampler_type(uniform.type)) {
					for (int j = 0; j < STAGE_MAX; j++) {
						r_gen_code.stage_globals[j] += ucode;
					}

					GeneratedCode::Texture texture;
					texture.name = uniform_name;
					texture.hint = uniform.hint;
					texture.type = uniform.type;
					texture.use_color = uniform.use_color;
					texture.filter = uniform.filter;
					texture.repeat = uniform.repeat;
					texture.global = uniform.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_GLOBAL;
					texture.array_size = uniform.array_size;
					if (texture.global) {
						r_gen_code.uses_global_textures = true;
					}

					r_gen_code.texture_uniforms.write[uniform.texture_order] = texture;
				} else {
					if (!uses_uniforms) {
						uses_uniforms = true;
					}
					uniform_defines.write[uniform.order] = ucode;
					if (is_buffer_global) {
						//globals are indices into the global table
						uniform_sizes.write[uniform.order] = ShaderLanguage::get_datatype_size(ShaderLanguage::TYPE_UINT);
						uniform_alignments.write[uniform.order] = _get_datatype_alignment(ShaderLanguage::TYPE_UINT);
					} else {
						// The following code enforces a 16-byte alignment of uniform arrays.
						if (uniform.array_size > 0) {
							int size = ShaderLanguage::get_datatype_size(uniform.type) * uniform.array_size;
							int m = (16 * uniform.array_size);
							if ((size % m) != 0) {
								size += m - (size % m);
							}
							uniform_sizes.write[uniform.order] = size;
							uniform_alignments.write[uniform.order] = 16;
						} else {
							uniform_sizes.write[uniform.order] = ShaderLanguage::get_datatype_size(uniform.type);
							uniform_alignments.write[uniform.order] = _get_datatype_alignment(uniform.type);
						}
					}
				}

				p_actions.uniforms->insert(uniform_name, uniform);
			}

			for (int i = 0; i < max_uniforms; i++) {
				r_gen_code.uniforms += uniform_defines[i];
			}

			// add up
			int offset = 0;
			for (int i = 0; i < uniform_sizes.size(); i++) {
				int align = offset % uniform_alignments[i];

				if (align != 0) {
					offset += uniform_alignments[i] - align;
				}

				r_gen_code.uniform_offsets.push_back(offset);

				offset += uniform_sizes[i];
			}

			r_gen_code.uniform_total_size = offset;

			if (r_gen_code.uniform_total_size % 16 != 0) { //UBO sizes must be multiples of 16
				r_gen_code.uniform_total_size += 16 - (r_gen_code.uniform_total_size % 16);
			}

			uint32_t index = p_default_actions.base_varying_index;

			List<Pair<StringName, SL::ShaderNode::Varying>> var_frag_to_light;

			Vector<StringName> varying_names;

			for (const KeyValue<StringName, SL::ShaderNode::Varying> &E : pnode->varyings) {
				varying_names.push_back(E.key);
			}

			varying_names.sort_custom<StringName::AlphCompare>(); //ensure order is deterministic so the same shader is always produced

			for (int k = 0; k < varying_names.size(); k++) {
				const StringName &varying_name = varying_names[k];
				const SL::ShaderNode::Varying &varying = pnode->varyings[varying_name];

				if (varying.stage == SL::ShaderNode::Varying::STAGE_FRAGMENT_TO_LIGHT || varying.stage == SL::ShaderNode::Varying::STAGE_FRAGMENT) {
					var_frag_to_light.push_back(Pair<StringName, SL::ShaderNode::Varying>(varying_name, varying));
					fragment_varyings.insert(varying_name);
					continue;
				}
				if (varying.type < SL::TYPE_INT) {
					continue; // Ignore boolean types to prevent crashing (if varying is just declared).
				}

				String vcode;
				String interp_mode = _interpstr(varying.interpolation);
				vcode += _prestr(varying.precision, ShaderLanguage::is_float_type(varying.type));
				vcode += _typestr(varying.type);
				vcode += " " + _mkid(varying_name);
				uint32_t inc = 1U;

				if (varying.array_size > 0) {
					inc = (uint32_t)varying.array_size;

					vcode += "[";
					vcode += itos(varying.array_size);
					vcode += "]";
				}

				switch (varying.type) {
					case SL::TYPE_MAT2:
						inc *= 2U;
						break;
					case SL::TYPE_MAT3:
						inc *= 3U;
						break;
					case SL::TYPE_MAT4:
						inc *= 4U;
						break;
					default:
						break;
				}

				vcode += ";\n";
				// GLSL ES 3.0 does not allow layout qualifiers for varyings
				if (!RS::get_singleton()->is_low_end()) {
					r_gen_code.stage_globals[STAGE_VERTEX] += "layout(location=" + itos(index) + ") ";
					r_gen_code.stage_globals[STAGE_FRAGMENT] += "layout(location=" + itos(index) + ") ";
				}
				r_gen_code.stage_globals[STAGE_VERTEX] += interp_mode + "out " + vcode;
				r_gen_code.stage_globals[STAGE_FRAGMENT] += interp_mode + "in " + vcode;

				index += inc;
			}

			if (var_frag_to_light.size() > 0) {
				String gcode = "\n\nstruct {\n";
				for (const Pair<StringName, SL::ShaderNode::Varying> &E : var_frag_to_light) {
					gcode += "\t" + _prestr(E.second.precision) + _typestr(E.second.type) + " " + _mkid(E.first);
					if (E.second.array_size > 0) {
						gcode += "[";
						gcode += itos(E.second.array_size);
						gcode += "]";
					}
					gcode += ";\n";
				}
				gcode += "} frag_to_light;\n";
				r_gen_code.stage_globals[STAGE_FRAGMENT] += gcode;
			}

			for (int i = 0; i < pnode->vconstants.size(); i++) {
				const SL::ShaderNode::Constant &cnode = pnode->vconstants[i];
				String gcode;
				gcode += _constr(true);
				gcode += _prestr(cnode.precision, ShaderLanguage::is_float_type(cnode.type));
				if (cnode.type == SL::TYPE_STRUCT) {
					gcode += _mkid(cnode.type_str);
				} else {
					gcode += _typestr(cnode.type);
				}
				gcode += " " + _mkid(String(cnode.name));
				if (cnode.array_size > 0) {
					gcode += "[";
					gcode += itos(cnode.array_size);
					gcode += "]";
				}
				gcode += "=";
				gcode += _dump_node_code(cnode.initializer, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				gcode += ";\n";
				for (int j = 0; j < STAGE_MAX; j++) {
					r_gen_code.stage_globals[j] += gcode;
				}
			}

			HashMap<StringName, String> function_code;

			//code for functions
			for (int i = 0; i < pnode->vfunctions.size(); i++) {
				SL::FunctionNode *fnode = pnode->vfunctions[i].function;
				function = fnode;
				current_func_name = fnode->name;
				function_code[fnode->name] = _dump_node_code(fnode->body, p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
				function = nullptr;
			}

			//place functions in actual code

			HashSet<StringName> added_funcs_per_stage[STAGE_MAX];

			for (int i = 0; i < pnode->vfunctions.size(); i++) {
				SL::FunctionNode *fnode = pnode->vfunctions[i].function;

				function = fnode;

				current_func_name = fnode->name;

				if (p_actions.entry_point_stages.has(fnode->name)) {
					Stage stage = p_actions.entry_point_stages[fnode->name];
					_dump_function_deps(pnode, fnode->name, function_code, r_gen_code.stage_globals[stage], added_funcs_per_stage[stage]);
					r_gen_code.code[fnode->name] = function_code[fnode->name];
				}

				function = nullptr;
			}

			//code+=dump_node_code(pnode->body,p_level);
		} break;
		case SL::Node::NODE_TYPE_STRUCT: {
		} break;
		case SL::Node::NODE_TYPE_FUNCTION: {
		} break;
		case SL::Node::NODE_TYPE_BLOCK: {
			SL::BlockNode *bnode = (SL::BlockNode *)p_node;

			//variables
			if (!bnode->single_statement) {
				code += _mktab(p_level - 1) + "{\n";
			}

			for (int i = 0; i < bnode->statements.size(); i++) {
				String scode = _dump_node_code(bnode->statements[i], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);

				if (bnode->statements[i]->type == SL::Node::NODE_TYPE_CONTROL_FLOW || bnode->single_statement) {
					code += scode; //use directly
					if (bnode->use_comma_between_statements && i + 1 < bnode->statements.size()) {
						code += ",";
					}
				} else {
					code += _mktab(p_level) + scode + ";\n";
				}
			}
			if (!bnode->single_statement) {
				code += _mktab(p_level - 1) + "}\n";
			}

		} break;
		case SL::Node::NODE_TYPE_VARIABLE_DECLARATION: {
			SL::VariableDeclarationNode *vdnode = (SL::VariableDeclarationNode *)p_node;

			String declaration;
			declaration += _constr(vdnode->is_const);
			if (vdnode->datatype == SL::TYPE_STRUCT) {
				declaration += _mkid(vdnode->struct_name);
			} else {
				declaration += _prestr(vdnode->precision) + _typestr(vdnode->datatype);
			}
			declaration += " ";
			for (int i = 0; i < vdnode->declarations.size(); i++) {
				bool is_array = vdnode->declarations[i].size > 0;
				if (i > 0) {
					declaration += ",";
				}
				declaration += _mkid(vdnode->declarations[i].name);
				if (is_array) {
					declaration += "[";
					if (vdnode->declarations[i].size_expression != nullptr) {
						declaration += _dump_node_code(vdnode->declarations[i].size_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					} else {
						declaration += itos(vdnode->declarations[i].size);
					}
					declaration += "]";
				}

				if (!is_array || vdnode->declarations[i].single_expression) {
					if (!vdnode->declarations[i].initializer.is_empty()) {
						declaration += "=";
						declaration += _dump_node_code(vdnode->declarations[i].initializer[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					}
				} else {
					int size = vdnode->declarations[i].initializer.size();
					if (size > 0) {
						declaration += "=";
						if (vdnode->datatype == SL::TYPE_STRUCT) {
							declaration += _mkid(vdnode->struct_name);
						} else {
							declaration += _typestr(vdnode->datatype);
						}
						declaration += "[";
						declaration += itos(size);
						declaration += "]";
						declaration += "(";
						for (int j = 0; j < size; j++) {
							if (j > 0) {
								declaration += ",";
							}
							declaration += _dump_node_code(vdnode->declarations[i].initializer[j], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
						}
						declaration += ")";
					}
				}
			}

			code += declaration;
		} break;
		case SL::Node::NODE_TYPE_VARIABLE: {
			SL::VariableNode *vnode = (SL::VariableNode *)p_node;
			bool use_fragment_varying = false;

			if (!vnode->is_local && !(p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_VERTEX)) {
				if (p_assigning) {
					if (shader->varyings.has(vnode->name)) {
						use_fragment_varying = true;
					}
				} else {
					if (fragment_varyings.has(vnode->name)) {
						use_fragment_varying = true;
					}
				}
			}

			if (p_assigning && p_actions.write_flag_pointers.has(vnode->name)) {
				*p_actions.write_flag_pointers[vnode->name] = true;
			}

			if (p_default_actions.usage_defines.has(vnode->name) && !used_name_defines.has(vnode->name)) {
				String define = p_default_actions.usage_defines[vnode->name];
				if (define.begins_with("@")) {
					define = p_default_actions.usage_defines[define.substr(1, define.length())];
				}
				r_gen_code.defines.push_back(define);
				used_name_defines.insert(vnode->name);
			}

			if (p_actions.usage_flag_pointers.has(vnode->name) && !used_flag_pointers.has(vnode->name)) {
				*p_actions.usage_flag_pointers[vnode->name] = true;
				used_flag_pointers.insert(vnode->name);
			}

			if (p_default_actions.renames.has(vnode->name)) {
				code = p_default_actions.renames[vnode->name];
			} else {
				if (shader->uniforms.has(vnode->name)) {
					//its a uniform!
					const ShaderLanguage::ShaderNode::Uniform &u = shader->uniforms[vnode->name];
					if (u.texture_order >= 0) {
						StringName name;
						if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SCREEN_TEXTURE) {
							name = "color_buffer";
							if (u.filter >= ShaderLanguage::FILTER_NEAREST_MIPMAP) {
								r_gen_code.uses_screen_texture_mipmaps = true;
							}
							r_gen_code.uses_screen_texture = true;
						} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE) {
							name = "normal_roughness_buffer";
							r_gen_code.uses_normal_roughness_texture = true;
						} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
							name = "depth_buffer";
							r_gen_code.uses_depth_texture = true;
						} else {
							name = _mkid(vnode->name); //texture, use as is
						}

						code = name;
					} else {
						//a scalar or vector
						if (u.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_GLOBAL) {
							code = actions.base_uniform_string + _mkid(vnode->name); //texture, use as is
							//global variable, this means the code points to an index to the global table
							code = _get_global_shader_uniform_from_type_and_index(p_default_actions.global_buffer_array_variable, code, u.type);
						} else if (u.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_INSTANCE) {
							//instance variable, index it as such
							code = "(" + p_default_actions.instance_uniform_index_variable + "+" + itos(u.instance_index) + ")";
							code = _get_global_shader_uniform_from_type_and_index(p_default_actions.global_buffer_array_variable, code, u.type);
						} else {
							//regular uniform, index from UBO
							code = actions.base_uniform_string + _mkid(vnode->name);
						}
					}

				} else {
					if (use_fragment_varying) {
						code = "frag_to_light.";
					}
					code += _mkid(vnode->name); //its something else (local var most likely) use as is
				}
			}

			if (vnode->name == time_name) {
				if (p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_VERTEX) {
					r_gen_code.uses_vertex_time = true;
				}
				if (p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_FRAGMENT) {
					r_gen_code.uses_fragment_time = true;
				}
			}

		} break;
		case SL::Node::NODE_TYPE_ARRAY_CONSTRUCT: {
			SL::ArrayConstructNode *acnode = (SL::ArrayConstructNode *)p_node;
			int sz = acnode->initializer.size();
			if (acnode->datatype == SL::TYPE_STRUCT) {
				code += _mkid(acnode->struct_name);
			} else {
				code += _typestr(acnode->datatype);
			}
			code += "[";
			code += itos(acnode->initializer.size());
			code += "]";
			code += "(";
			for (int i = 0; i < sz; i++) {
				code += _dump_node_code(acnode->initializer[i], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				if (i != sz - 1) {
					code += ", ";
				}
			}
			code += ")";
		} break;
		case SL::Node::NODE_TYPE_ARRAY: {
			SL::ArrayNode *anode = (SL::ArrayNode *)p_node;
			bool use_fragment_varying = false;

			if (!anode->is_local && !(p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_VERTEX)) {
				if (anode->assign_expression != nullptr && shader->varyings.has(anode->name)) {
					use_fragment_varying = true;
				} else {
					if (p_assigning) {
						if (shader->varyings.has(anode->name)) {
							use_fragment_varying = true;
						}
					} else {
						if (fragment_varyings.has(anode->name)) {
							use_fragment_varying = true;
						}
					}
				}
			}

			if (p_assigning && p_actions.write_flag_pointers.has(anode->name)) {
				*p_actions.write_flag_pointers[anode->name] = true;
			}

			if (p_default_actions.usage_defines.has(anode->name) && !used_name_defines.has(anode->name)) {
				String define = p_default_actions.usage_defines[anode->name];
				if (define.begins_with("@")) {
					define = p_default_actions.usage_defines[define.substr(1, define.length())];
				}
				r_gen_code.defines.push_back(define);
				used_name_defines.insert(anode->name);
			}

			if (p_actions.usage_flag_pointers.has(anode->name) && !used_flag_pointers.has(anode->name)) {
				*p_actions.usage_flag_pointers[anode->name] = true;
				used_flag_pointers.insert(anode->name);
			}

			if (p_default_actions.renames.has(anode->name)) {
				code = p_default_actions.renames[anode->name];
			} else {
				if (shader->uniforms.has(anode->name)) {
					//its a uniform!
					const ShaderLanguage::ShaderNode::Uniform &u = shader->uniforms[anode->name];
					if (u.texture_order >= 0) {
						code = _mkid(anode->name); //texture, use as is
					} else {
						//a scalar or vector
						if (u.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_GLOBAL) {
							code = actions.base_uniform_string + _mkid(anode->name); //texture, use as is
							//global variable, this means the code points to an index to the global table
							code = _get_global_shader_uniform_from_type_and_index(p_default_actions.global_buffer_array_variable, code, u.type);
						} else if (u.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_INSTANCE) {
							//instance variable, index it as such
							code = "(" + p_default_actions.instance_uniform_index_variable + "+" + itos(u.instance_index) + ")";
							code = _get_global_shader_uniform_from_type_and_index(p_default_actions.global_buffer_array_variable, code, u.type);
						} else {
							//regular uniform, index from UBO
							code = actions.base_uniform_string + _mkid(anode->name);
						}
					}
				} else {
					if (use_fragment_varying) {
						code = "frag_to_light.";
					}
					code += _mkid(anode->name);
				}
			}

			if (anode->call_expression != nullptr) {
				code += ".";
				code += _dump_node_code(anode->call_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning, false);
			} else if (anode->index_expression != nullptr) {
				code += "[";
				code += _dump_node_code(anode->index_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				code += "]";
			} else if (anode->assign_expression != nullptr) {
				code += "=";
				code += _dump_node_code(anode->assign_expression, p_level, r_gen_code, p_actions, p_default_actions, true, false);
			}

			if (anode->name == time_name) {
				if (p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_VERTEX) {
					r_gen_code.uses_vertex_time = true;
				}
				if (p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_FRAGMENT) {
					r_gen_code.uses_fragment_time = true;
				}
			}

		} break;
		case SL::Node::NODE_TYPE_CONSTANT: {
			SL::ConstantNode *cnode = (SL::ConstantNode *)p_node;

			if (cnode->array_size == 0) {
				return get_constant_text(cnode->datatype, cnode->values);
			} else {
				if (cnode->get_datatype() == SL::TYPE_STRUCT) {
					code += _mkid(cnode->struct_name);
				} else {
					code += _typestr(cnode->datatype);
				}
				code += "[";
				code += itos(cnode->array_size);
				code += "]";
				code += "(";
				for (int i = 0; i < cnode->array_size; i++) {
					if (i > 0) {
						code += ",";
					} else {
						code += "";
					}
					code += _dump_node_code(cnode->array_declarations[0].initializer[i], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				}
				code += ")";
			}

		} break;
		case SL::Node::NODE_TYPE_OPERATOR: {
			SL::OperatorNode *onode = (SL::OperatorNode *)p_node;

			switch (onode->op) {
				case SL::OP_ASSIGN:
				case SL::OP_ASSIGN_ADD:
				case SL::OP_ASSIGN_SUB:
				case SL::OP_ASSIGN_MUL:
				case SL::OP_ASSIGN_DIV:
				case SL::OP_ASSIGN_SHIFT_LEFT:
				case SL::OP_ASSIGN_SHIFT_RIGHT:
				case SL::OP_ASSIGN_MOD:
				case SL::OP_ASSIGN_BIT_AND:
				case SL::OP_ASSIGN_BIT_OR:
				case SL::OP_ASSIGN_BIT_XOR:
					code = _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, true) + _opstr(onode->op) + _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					break;
				case SL::OP_BIT_INVERT:
				case SL::OP_NEGATE:
				case SL::OP_NOT:
				case SL::OP_DECREMENT:
				case SL::OP_INCREMENT:
					code = _opstr(onode->op) + _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					break;
				case SL::OP_POST_DECREMENT:
				case SL::OP_POST_INCREMENT:
					code = _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + _opstr(onode->op);
					break;
				case SL::OP_CALL:
				case SL::OP_STRUCT:
				case SL::OP_CONSTRUCT: {
					ERR_FAIL_COND_V(onode->arguments[0]->type != SL::Node::NODE_TYPE_VARIABLE, String());
					const SL::VariableNode *vnode = static_cast<const SL::VariableNode *>(onode->arguments[0]);
					const SL::FunctionNode *func = nullptr;
					const bool is_internal_func = internal_functions.has(vnode->name);

					if (!is_internal_func) {
						for (int i = 0; i < shader->vfunctions.size(); i++) {
							if (shader->vfunctions[i].name == vnode->name) {
								func = shader->vfunctions[i].function;
								break;
							}
						}
					}

					bool is_texture_func = false;
					bool is_screen_texture = false;
					bool texture_func_no_uv = false;
					bool texture_func_returns_data = false;

					if (onode->op == SL::OP_STRUCT) {
						code += _mkid(vnode->name);
					} else if (onode->op == SL::OP_CONSTRUCT) {
						code += String(vnode->name);
					} else {
						if (p_actions.usage_flag_pointers.has(vnode->name) && !used_flag_pointers.has(vnode->name)) {
							*p_actions.usage_flag_pointers[vnode->name] = true;
							used_flag_pointers.insert(vnode->name);
						}

						if (is_internal_func) {
							code += vnode->name;
							is_texture_func = texture_functions.has(vnode->name);
							texture_func_no_uv = (vnode->name == "textureSize" || vnode->name == "textureQueryLevels");
							texture_func_returns_data = texture_func_no_uv || vnode->name == "textureQueryLod";
						} else if (p_default_actions.renames.has(vnode->name)) {
							code += p_default_actions.renames[vnode->name];
						} else {
							code += _mkid(vnode->name);
						}
					}

					code += "(";

					// if color backbuffer, depth backbuffer or normal roughness texture is used,
					// we will add logic to automatically switch between
					// sampler2D and sampler2D array and vec2 UV and vec3 UV.
					bool multiview_uv_needed = false;
					bool is_normal_roughness_texture = false;

					for (int i = 1; i < onode->arguments.size(); i++) {
						if (i > 1) {
							code += ", ";
						}

						bool is_out_qualifier = false;
						if (is_internal_func) {
							is_out_qualifier = SL::is_builtin_func_out_parameter(vnode->name, i - 1);
						} else if (func != nullptr) {
							const SL::ArgumentQualifier qualifier = func->arguments[i - 1].qualifier;
							is_out_qualifier = qualifier == SL::ARGUMENT_QUALIFIER_OUT || qualifier == SL::ARGUMENT_QUALIFIER_INOUT;
						}

						if (is_out_qualifier) {
							StringName name;
							bool found = false;
							{
								const SL::Node *node = onode->arguments[i];

								bool done = false;
								do {
									switch (node->type) {
										case SL::Node::NODE_TYPE_VARIABLE: {
											name = static_cast<const SL::VariableNode *>(node)->name;
											done = true;
											found = true;
										} break;
										case SL::Node::NODE_TYPE_MEMBER: {
											node = static_cast<const SL::MemberNode *>(node)->owner;
										} break;
										default: {
											done = true;
										} break;
									}
								} while (!done);
							}

							if (found && p_actions.write_flag_pointers.has(name)) {
								*p_actions.write_flag_pointers[name] = true;
							}
						}

						String node_code = _dump_node_code(onode->arguments[i], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
						if (is_texture_func && i == 1) {
							// If we're doing a texture lookup we need to check our texture argument
							StringName texture_uniform;
							bool correct_texture_uniform = false;

							switch (onode->arguments[i]->type) {
								case SL::Node::NODE_TYPE_VARIABLE: {
									const SL::VariableNode *varnode = static_cast<const SL::VariableNode *>(onode->arguments[i]);
									texture_uniform = varnode->name;
									correct_texture_uniform = true;
								} break;
								case SL::Node::NODE_TYPE_ARRAY: {
									const SL::ArrayNode *anode = static_cast<const SL::ArrayNode *>(onode->arguments[i]);
									texture_uniform = anode->name;
									correct_texture_uniform = true;
								} break;
								default:
									break;
							}

							if (correct_texture_uniform && !RS::get_singleton()->is_low_end()) {
								// Need to map from texture to sampler in order to sample when using Vulkan GLSL.
								String sampler_name;
								bool is_depth_texture = false;

								if (actions.custom_samplers.has(texture_uniform)) {
									sampler_name = actions.custom_samplers[texture_uniform];
								} else {
									if (shader->uniforms.has(texture_uniform)) {
										const ShaderLanguage::ShaderNode::Uniform &u = shader->uniforms[texture_uniform];
										if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SCREEN_TEXTURE) {
											is_screen_texture = true;
										} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
											is_depth_texture = true;
										} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE) {
											is_normal_roughness_texture = true;
										}
										sampler_name = _get_sampler_name(u.filter, u.repeat);
									} else {
										bool found = false;

										for (int j = 0; j < function->arguments.size(); j++) {
											if (function->arguments[j].name == texture_uniform) {
												if (function->arguments[j].tex_builtin_check) {
													ERR_CONTINUE(!actions.custom_samplers.has(function->arguments[j].tex_builtin));
													sampler_name = actions.custom_samplers[function->arguments[j].tex_builtin];
													found = true;
													break;
												}
												if (function->arguments[j].tex_argument_check) {
													sampler_name = _get_sampler_name(function->arguments[j].tex_argument_filter, function->arguments[j].tex_argument_repeat);
													found = true;
													break;
												}
											}
										}
										if (!found) {
											//function was most likely unused, so use anything (compiler will remove it anyway)
											sampler_name = _get_sampler_name(ShaderLanguage::FILTER_DEFAULT, ShaderLanguage::REPEAT_DEFAULT);
										}
									}
								}

								String data_type_name = "";
								if (actions.check_multiview_samplers && (is_screen_texture || is_depth_texture || is_normal_roughness_texture)) {
									data_type_name = "multiviewSampler";
									multiview_uv_needed = true;
								} else {
									data_type_name = ShaderLanguage::get_datatype_name(onode->arguments[i]->get_datatype());
								}

								code += data_type_name + "(" + node_code + ", " + sampler_name + ")";
							} else if (actions.check_multiview_samplers && correct_texture_uniform && RS::get_singleton()->is_low_end()) {
								// Texture function on low end hardware (i.e. OpenGL).
								// We just need to know if the texture supports multiview.

								if (shader->uniforms.has(texture_uniform)) {
									const ShaderLanguage::ShaderNode::Uniform &u = shader->uniforms[texture_uniform];
									if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SCREEN_TEXTURE) {
										multiview_uv_needed = true;
									} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
										multiview_uv_needed = true;
									} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE) {
										multiview_uv_needed = true;
									}
								}

								code += node_code;
							} else {
								code += node_code;
							}
						} else if (multiview_uv_needed && !texture_func_no_uv && i == 2) {
							// UV coordinate after using color, depth or normal roughness texture.
							node_code = "multiview_uv(" + node_code + ".xy)";

							code += node_code;
						} else {
							code += node_code;
						}
					}
					code += ")";
					if (is_screen_texture && !texture_func_returns_data && actions.apply_luminance_multiplier) {
						code = "(" + code + " * vec4(vec3(sc_luminance_multiplier), 1.0))";
					}
					if (is_normal_roughness_texture && !texture_func_returns_data) {
						code = "normal_roughness_compatibility(" + code + ")";
					}
				} break;
				case SL::OP_INDEX: {
					code += _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					code += "[";
					code += _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					code += "]";

				} break;
				case SL::OP_SELECT_IF: {
					code += "(";
					code += _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					code += "?";
					code += _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					code += ":";
					code += _dump_node_code(onode->arguments[2], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					code += ")";

				} break;
				case SL::OP_EMPTY: {
					// Semicolon (or empty statement) - ignored.
				} break;

				default: {
					if (p_use_scope) {
						code += "(";
					}
					code += _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + " " + _opstr(onode->op) + " " + _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					if (p_use_scope) {
						code += ")";
					}
					break;
				}
			}

		} break;
		case SL::Node::NODE_TYPE_CONTROL_FLOW: {
			SL::ControlFlowNode *cfnode = (SL::ControlFlowNode *)p_node;
			if (cfnode->flow_op == SL::FLOW_OP_IF) {
				code += _mktab(p_level) + "if (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ")\n";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
				if (cfnode->blocks.size() == 2) {
					code += _mktab(p_level) + "else\n";
					code += _dump_node_code(cfnode->blocks[1], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
				}
			} else if (cfnode->flow_op == SL::FLOW_OP_SWITCH) {
				code += _mktab(p_level) + "switch (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ")\n";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
			} else if (cfnode->flow_op == SL::FLOW_OP_CASE) {
				code += _mktab(p_level) + "case " + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ":\n";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
			} else if (cfnode->flow_op == SL::FLOW_OP_DEFAULT) {
				code += _mktab(p_level) + "default:\n";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
			} else if (cfnode->flow_op == SL::FLOW_OP_DO) {
				code += _mktab(p_level) + "do";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
				code += _mktab(p_level) + "while (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ");";
			} else if (cfnode->flow_op == SL::FLOW_OP_WHILE) {
				code += _mktab(p_level) + "while (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ")\n";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
			} else if (cfnode->flow_op == SL::FLOW_OP_FOR) {
				String left = _dump_node_code(cfnode->blocks[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				String middle = _dump_node_code(cfnode->blocks[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				String right = _dump_node_code(cfnode->blocks[2], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				code += _mktab(p_level) + "for (" + left + ";" + middle + ";" + right + ")\n";
				code += _dump_node_code(cfnode->blocks[3], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);

			} else if (cfnode->flow_op == SL::FLOW_OP_RETURN) {
				if (cfnode->expressions.size()) {
					code = "return " + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ";";
				} else {
					code = "return;";
				}
			} else if (cfnode->flow_op == SL::FLOW_OP_DISCARD) {
				if (p_actions.usage_flag_pointers.has("DISCARD") && !used_flag_pointers.has("DISCARD")) {
					*p_actions.usage_flag_pointers["DISCARD"] = true;
					used_flag_pointers.insert("DISCARD");
				}

				code = "discard;";
			} else if (cfnode->flow_op == SL::FLOW_OP_CONTINUE) {
				code = "continue;";
			} else if (cfnode->flow_op == SL::FLOW_OP_BREAK) {
				code = "break;";
			}

		} break;
		case SL::Node::NODE_TYPE_MEMBER: {
			SL::MemberNode *mnode = (SL::MemberNode *)p_node;
			code = _dump_node_code(mnode->owner, p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + "." + mnode->name;
			if (mnode->index_expression != nullptr) {
				code += "[";
				code += _dump_node_code(mnode->index_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				code += "]";
			} else if (mnode->assign_expression != nullptr) {
				code += "=";
				code += _dump_node_code(mnode->assign_expression, p_level, r_gen_code, p_actions, p_default_actions, true, false);
			} else if (mnode->call_expression != nullptr) {
				code += ".";
				code += _dump_node_code(mnode->call_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning, false);
			}
		} break;
	}

	return code;
}

ShaderLanguage::DataType ShaderCompiler::_get_global_shader_uniform_type(const StringName &p_name) {
	RS::GlobalShaderParameterType gvt = RSG::material_storage->global_shader_parameter_get_type(p_name);
	return (ShaderLanguage::DataType)RS::global_shader_uniform_type_get_shader_datatype(gvt);
}

Error ShaderCompiler::compile(RS::ShaderMode p_mode, const String &p_code, IdentifierActions *p_actions, const String &p_path, GeneratedCode &r_gen_code) {
	SL::ShaderCompileInfo info;
	info.functions = ShaderTypes::get_singleton()->get_functions(p_mode);
	info.render_modes = ShaderTypes::get_singleton()->get_modes(p_mode);
	info.shader_types = ShaderTypes::get_singleton()->get_types();
	info.global_shader_uniform_type_func = _get_global_shader_uniform_type;

	Error err = parser.compile(p_code, info);

	if (err != OK) {
		Vector<ShaderLanguage::FilePosition> include_positions = parser.get_include_positions();

		String current;
		HashMap<String, Vector<String>> includes;
		includes[""] = Vector<String>();
		Vector<String> include_stack;
		Vector<String> shader_lines = p_code.split("\n");

		// Reconstruct the files.
		for (int i = 0; i < shader_lines.size(); i++) {
			String l = shader_lines[i];
			if (l.begins_with("@@>")) {
				String inc_path = l.replace_first("@@>", "");

				l = "#include \"" + inc_path + "\"";
				includes[current].append("#include \"" + inc_path + "\""); // Restore the include directive
				include_stack.push_back(current);
				current = inc_path;
				includes[inc_path] = Vector<String>();

			} else if (l.begins_with("@@<")) {
				if (include_stack.size()) {
					current = include_stack[include_stack.size() - 1];
					include_stack.resize(include_stack.size() - 1);
				}
			} else {
				includes[current].push_back(l);
			}
		}

		// Print the files.
		for (const KeyValue<String, Vector<String>> &E : includes) {
			if (E.key.is_empty()) {
				if (p_path == "") {
					print_line("--Main Shader--");
				} else {
					print_line("--" + p_path + "--");
				}
			} else {
				print_line("--" + E.key + "--");
			}
			int err_line = -1;
			for (int i = 0; i < include_positions.size(); i++) {
				if (include_positions[i].file == E.key) {
					err_line = include_positions[i].line;
				}
			}
			const Vector<String> &V = E.value;
			for (int i = 0; i < V.size(); i++) {
				if (i == err_line - 1) {
					// Mark the error line to be visible without having to look at
					// the trace at the end.
					print_line(vformat("E%4d-> %s", i + 1, V[i]));
				} else {
					print_line(vformat("%5d | %s", i + 1, V[i]));
				}
			}
		}

		String file;
		int line;
		if (include_positions.size() > 1) {
			file = include_positions[include_positions.size() - 1].file;
			line = include_positions[include_positions.size() - 1].line;
		} else {
			file = p_path;
			line = parser.get_error_line();
		}

		_err_print_error(nullptr, file.utf8().get_data(), line, parser.get_error_text().utf8().get_data(), false, ERR_HANDLER_SHADER);
		return err;
	}

	r_gen_code.defines.clear();
	r_gen_code.code.clear();
	for (int i = 0; i < STAGE_MAX; i++) {
		r_gen_code.stage_globals[i] = String();
	}
	r_gen_code.uses_fragment_time = false;
	r_gen_code.uses_vertex_time = false;
	r_gen_code.uses_global_textures = false;
	r_gen_code.uses_screen_texture_mipmaps = false;
	r_gen_code.uses_screen_texture = false;
	r_gen_code.uses_depth_texture = false;
	r_gen_code.uses_normal_roughness_texture = false;

	used_name_defines.clear();
	used_rmode_defines.clear();
	used_flag_pointers.clear();
	fragment_varyings.clear();

	shader = parser.get_shader();
	function = nullptr;
	_dump_node_code(shader, 1, r_gen_code, *p_actions, actions, false);

	return OK;
}

void ShaderCompiler::initialize(DefaultIdentifierActions p_actions) {
	actions = p_actions;

	time_name = "TIME";

	List<String> func_list;

	ShaderLanguage::get_builtin_funcs(&func_list);

	for (const String &E : func_list) {
		internal_functions.insert(E);
	}
	texture_functions.insert("texture");
	texture_functions.insert("textureProj");
	texture_functions.insert("textureLod");
	texture_functions.insert("textureProjLod");
	texture_functions.insert("textureGrad");
	texture_functions.insert("textureProjGrad");
	texture_functions.insert("textureGather");
	texture_functions.insert("textureSize");
	texture_functions.insert("textureQueryLod");
	texture_functions.insert("textureQueryLevels");
	texture_functions.insert("texelFetch");
}

ShaderCompiler::ShaderCompiler() {
}

#ifndef DISABLE_DEPRECATED

static const char *old_builtin_funcs[]{
	"abs",
	"acos",
	"acosh",
	"all",
	"any",
	"asin",
	"asinh",
	"atan",
	"atanh",
	"bool",
	"bvec2",
	"bvec3",
	"bvec4",
	"ceil",
	"clamp",
	"cos",
	"cosh",
	"cross",
	"dFdx",
	"dFdy",
	"degrees",
	"determinant",
	"distance",
	"dot",
	"equal",
	"exp",
	"exp2",
	"faceforward",
	"float",
	"floatBitsToInt",
	"floatBitsToUint",
	"floor",
	"fract",
	"fwidth",
	"greaterThan",
	"greaterThanEqual",
	"int",
	"intBitsToFloat",
	"inverse",
	"inversesqrt",
	"isinf",
	"isnan",
	"ivec2",
	"ivec3",
	"ivec4",
	"length",
	"lessThan",
	"lessThanEqual",
	"log",
	"log2",
	"mat2",
	"mat3",
	"mat4",
	"matrixCompMult",
	"max",
	"min",
	"mix",
	"mod",
	"modf",
	"normalize",
	"not",
	"notEqual",
	"outerProduct",
	"pow",
	"radians",
	"reflect",
	"refract",
	"round",
	"roundEven",
	"sign",
	"sin",
	"sinh",
	"smoothstep",
	"sqrt",
	"step",
	"tan",
	"tanh",
	"texelFetch",
	"texture",
	"textureGrad",
	"textureLod",
	"textureProj",
	"textureProjLod",
	"textureSize",
	"transpose",
	"trunc",
	"uint",
	"uintBitsToFloat",
	"uvec2",
	"uvec3",
	"uvec4",
	"vec2",
	"vec3",
	"vec4",
};

static HashSet<String> _construct_new_builtin_funcs() {
	List<String> current_builtin_funcs;
	ShaderLanguage::get_builtin_funcs(&current_builtin_funcs);
	HashSet<String> old_funcs;
	for (int i = 0; old_builtin_funcs[i] != nullptr; i++) {
		old_funcs.insert(old_builtin_funcs[i]);
	}
	HashSet<String> new_funcs;
	for (List<String>::Element *E = current_builtin_funcs.front(); E; E = E->next()) {
		if (!old_funcs.has(E->get())) {
			new_funcs.insert(E->get());
		}
	}
	return new_funcs;
}

const static HashSet<String> new_builtin_funcs = _construct_new_builtin_funcs();

const ShaderDeprecatedConverter::RenamedBuiltins ShaderDeprecatedConverter::renamed_builtins[] = {
	{ "ALPHA_SCISSOR", "ALPHA_SCISSOR_THRESHOLD", { { RS::SHADER_SPATIAL, { "fragment" } } }, false },
	{ "CAMERA_MATRIX", "INV_VIEW_MATRIX", { { RS::SHADER_SPATIAL, { "vertex", "fragment", "light" } } }, false },
	{ "INV_CAMERA_MATRIX", "VIEW_MATRIX", { { RS::SHADER_SPATIAL, { "vertex", "fragment", "light" } } }, false },
	{ "NORMALMAP", "NORMAL_MAP", { { RS::SHADER_CANVAS_ITEM, { "fragment" } }, { RS::SHADER_SPATIAL, { "fragment" } } }, false },
	{ "NORMALMAP_DEPTH", "NORMAL_MAP_DEPTH", { { RS::SHADER_CANVAS_ITEM, { "fragment" } }, { RS::SHADER_SPATIAL, { "fragment" } } }, false },
	{ "TRANSMISSION", "BACKLIGHT", { { RS::SHADER_SPATIAL, { "fragment", "light" } } }, false },
	{ "WORLD_MATRIX", "MODEL_MATRIX", { { RS::SHADER_CANVAS_ITEM, { "vertex" } }, { RS::SHADER_SPATIAL, { "vertex", "fragment", "light" } } }, false },
	{ "CLEARCOAT_GLOSS", "CLEARCOAT_ROUGHNESS", { { RS::SHADER_SPATIAL, { "fragment" } } }, true }, // Usages require inversion, manually handled
	{ "INDEX", "INDEX", { { RS::SHADER_PARTICLES, { "vertex" } } }, true }, // No rename, was previously an int (vs. uint), usages require wrapping in `int()`.
	{ nullptr, nullptr, {}, false },
};

const ShaderDeprecatedConverter::RenamedRenderModes ShaderDeprecatedConverter::renamed_render_modes[] = {
	{ RS::SHADER_SPATIAL, "depth_draw_alpha_prepass", "depth_prepass_alpha" },
	{ RS::SHADER_MAX, nullptr, nullptr },
};

const ShaderDeprecatedConverter::RenamedHints ShaderDeprecatedConverter::renamed_hints[]{
	{ "hint_albedo", SL::TokenType::TK_HINT_SOURCE_COLOR },
	{ "hint_aniso", SL::TokenType::TK_HINT_ANISOTROPY_TEXTURE },
	{ "hint_black", SL::TokenType::TK_HINT_DEFAULT_BLACK_TEXTURE },
	{ "hint_black_albedo", SL::TokenType::TK_HINT_DEFAULT_BLACK_TEXTURE },
	{ "hint_color", SL::TokenType::TK_HINT_SOURCE_COLOR },
	{ "hint_transparent", SL::TokenType::TK_HINT_DEFAULT_TRANSPARENT_TEXTURE },
	{ "hint_white", SL::TokenType::TK_HINT_DEFAULT_WHITE_TEXTURE },
	{ nullptr, {} },
};

const ShaderDeprecatedConverter::RenamedFunctions ShaderDeprecatedConverter::renamed_functions[]{
	{ RS::SHADER_PARTICLES, SL::TK_TYPE_VOID, "vertex", "process" },
	{ RS::SHADER_MAX, SL::TK_EMPTY, nullptr, nullptr },
};

const ShaderDeprecatedConverter::RemovedRenderModes ShaderDeprecatedConverter::removed_render_modes[]{
	{ RS::SHADER_SPATIAL, "specular_blinn", false },
	{ RS::SHADER_SPATIAL, "specular_phong", false },
	{ RS::SHADER_SPATIAL, "async_visible", true },
	{ RS::SHADER_SPATIAL, "async_hidden", true },
	{ RS::SHADER_MAX, nullptr, false },
};

// These necessitate adding a uniform to the shader.
const ShaderDeprecatedConverter::RemovedBuiltins ShaderDeprecatedConverter::removed_builtins[]{
	{ "SCREEN_TEXTURE", SL::TK_TYPE_SAMPLER2D, { SL::TK_HINT_SCREEN_TEXTURE, SL::TK_FILTER_LINEAR_MIPMAP }, { { RS::SHADER_SPATIAL, { "fragment" } }, { RS::SHADER_CANVAS_ITEM, { "fragment" } } } },
	{ "DEPTH_TEXTURE", SL::TK_TYPE_SAMPLER2D, { SL::TK_HINT_DEPTH_TEXTURE, SL::TK_FILTER_LINEAR_MIPMAP }, { { RS::SHADER_SPATIAL, { "fragment" } } } },
	{ "NORMAL_ROUGHNESS_TEXTURE", SL::TK_TYPE_SAMPLER2D, { SL::TK_HINT_NORMAL_ROUGHNESS_TEXTURE, SL::TK_FILTER_LINEAR_MIPMAP }, { { RS::SHADER_SPATIAL, { "fragment" } } } },
	{ "MODULATE", SL::TK_ERROR, {}, { { RS::SHADER_CANVAS_ITEM, { "vertex", "fragment", "light" } } } }, // TODO: remove this when the MODULATE PR lands.
	{ nullptr, SL::TK_EMPTY, {}, {} },
};

const char *ShaderDeprecatedConverter::removed_types[]{
	"samplerExternalOES",
	nullptr,
};

String ShaderDeprecatedConverter::get_builtin_rename(const String &p_name) {
	for (int i = 0; renamed_builtins[i].name != nullptr; i++) {
		if (renamed_builtins[i].name == p_name) {
			return renamed_builtins[i].replacement;
		}
	}
	return {};
}

bool ShaderDeprecatedConverter::has_builtin_rename(RS::ShaderMode p_mode, const String &p_name, const String &p_function) {
	for (int i = 0; renamed_builtins[i].name != nullptr; i++) {
		if (renamed_builtins[i].name == p_name) {
			for (int j = 0; j < renamed_builtins[i].mode_functions.size(); j++) {
				if (renamed_builtins[i].mode_functions[j].first == p_mode) {
					if (p_function == "") { // Empty function means don't check function.
						return true;
					}
					for (int k = 0; k < renamed_builtins[i].mode_functions[j].second.size(); k++) {
						if (renamed_builtins[i].mode_functions[j].second[k] == p_function) {
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

SL::TokenType ShaderDeprecatedConverter::get_removed_builtin_type(const String &p_name) {
	for (int i = 0; removed_builtins[i].name != nullptr; i++) {
		if (removed_builtins[i].name == p_name) {
			return removed_builtins[i].uniform_type;
		}
	}
	return SL::TK_EMPTY;
}

Vector<SL::TokenType> ShaderDeprecatedConverter::get_removed_builtin_hints(const String &p_name) {
	for (int i = 0; removed_builtins[i].name != nullptr; i++) {
		if (removed_builtins[i].name == p_name) {
			return removed_builtins[i].hints;
		}
	}
	return {};
}

bool ShaderDeprecatedConverter::_rename_has_special_handling(const String &p_name) {
	for (int i = 0; renamed_builtins[i].name != nullptr; i++) {
		if (renamed_builtins[i].name == p_name) {
			return renamed_builtins[i].special_handling;
		}
	}
	return false;
}

void ShaderDeprecatedConverter::_get_builtin_renames_list(List<String> *r_list) {
	for (int i = 0; renamed_builtins[i].name != nullptr; i++) {
		r_list->push_back(renamed_builtins[i].name);
	}
}

void ShaderDeprecatedConverter::_get_render_mode_renames_list(List<String> *r_list) {
	for (int i = 0; renamed_render_modes[i].name != nullptr; i++) {
		r_list->push_back(renamed_render_modes[i].name);
	}
}

void ShaderDeprecatedConverter::_get_hint_renames_list(List<String> *r_list) {
	for (int i = 0; renamed_hints[i].name != nullptr; i++) {
		r_list->push_back(renamed_hints[i].name);
	}
}

void ShaderDeprecatedConverter::_get_function_renames_list(List<String> *r_list) {
	for (int i = 0; renamed_functions[i].name != nullptr; i++) {
		r_list->push_back(renamed_functions[i].name);
	}
}

void ShaderDeprecatedConverter::_get_render_mode_removals_list(List<String> *r_list) {
	for (int i = 0; removed_render_modes[i].name != nullptr; i++) {
		r_list->push_back(removed_render_modes[i].name);
	}
}

void ShaderDeprecatedConverter::_get_builtin_removals_list(List<String> *r_list) {
	for (int i = 0; removed_builtins[i].name != nullptr; i++) {
		r_list->push_back(removed_builtins[i].name);
	}
}

void ShaderDeprecatedConverter::_get_type_removals_list(List<String> *r_list) {
	for (int i = 0; removed_types[i] != nullptr; i++) {
		r_list->push_back(removed_types[i]);
	}
}

Vector<String> ShaderDeprecatedConverter::_get_funcs_builtin_rename(RS::ShaderMode p_mode, const String &p_name) {
	Vector<String> funcs;
	for (int i = 0; renamed_builtins[i].name != nullptr; i++) {
		if (renamed_builtins[i].name == p_name) {
			for (int j = 0; j < renamed_builtins[i].mode_functions.size(); j++) {
				if (renamed_builtins[i].mode_functions[j].first == p_mode) {
					for (int k = 0; k < renamed_builtins[i].mode_functions[j].second.size(); k++) {
						funcs.push_back(renamed_builtins[i].mode_functions[j].second[k]);
					}
				}
			}
		}
	}
	return funcs;
}

Vector<String> ShaderDeprecatedConverter::_get_funcs_builtin_removal(RS::ShaderMode p_mode, const String &p_name) {
	Vector<String> funcs;
	for (int i = 0; removed_builtins[i].name != nullptr; i++) {
		if (removed_builtins[i].name == p_name) {
			for (int j = 0; j < removed_builtins[i].mode_functions.size(); j++) {
				if (removed_builtins[i].mode_functions[j].first == p_mode) {
					for (int k = 0; k < removed_builtins[i].mode_functions[j].second.size(); k++) {
						funcs.push_back(removed_builtins[i].mode_functions[j].second[k]);
					}
				}
			}
		}
	}
	return funcs;
}

bool ShaderDeprecatedConverter::is_removed_builtin(RS::ShaderMode p_mode, const String &p_name, const String &p_function) {
	for (int i = 0; removed_builtins[i].name != nullptr; i++) {
		if (removed_builtins[i].name == p_name) {
			for (int j = 0; j < removed_builtins[i].mode_functions.size(); j++) {
				if (removed_builtins[i].mode_functions[j].first == p_mode) {
					if (p_function == "") { // Empty function means don't check function.
						return true;
					}
					for (int k = 0; k < removed_builtins[i].mode_functions[j].second.size(); k++) {
						if (removed_builtins[i].mode_functions[j].second[k] == p_function) {
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool ShaderDeprecatedConverter::has_hint_replacement(const String &p_name) {
	for (int i = 0; renamed_hints[i].name != nullptr; i++) {
		if (renamed_hints[i].name == p_name) {
			return true;
		}
	}
	return false;
}

SL::TokenType ShaderDeprecatedConverter::get_hint_replacement(const String &p_name) {
	for (int i = 0; renamed_hints[i].name != nullptr; i++) {
		if (renamed_hints[i].name == p_name) {
			return renamed_hints[i].replacement;
		}
	}
	return {};
}

bool ShaderDeprecatedConverter::is_renamed_render_mode(RS::ShaderMode p_mode, const String &p_name) {
	for (int i = 0; renamed_render_modes[i].name != nullptr; i++) {
		if (renamed_render_modes[i].mode == p_mode && renamed_render_modes[i].name == p_name) {
			return true;
		}
	}
	return false;
}

String ShaderDeprecatedConverter::get_render_mode_rename(const String &p_name) {
	for (int i = 0; renamed_render_modes[i].name != nullptr; i++) {
		if (renamed_render_modes[i].name == p_name) {
			return renamed_render_modes[i].replacement;
		}
	}
	return {};
}

bool ShaderDeprecatedConverter::is_renamed_function(RS::ShaderMode p_mode, const String &p_name) {
	for (int i = 0; renamed_functions[i].name != nullptr; i++) {
		if (renamed_functions[i].mode == p_mode && renamed_functions[i].name == p_name) {
			return true;
		}
	}
	return false;
}

SL::TokenType ShaderDeprecatedConverter::get_renamed_function_type(const String &p_name) {
	for (int i = 0; renamed_functions[i].name != nullptr; i++) {
		if (renamed_functions[i].name == p_name) {
			return renamed_functions[i].type;
		}
	}
	return SL::TK_MAX;
}

String ShaderDeprecatedConverter::get_renamed_function(const String &p_name) {
	for (int i = 0; renamed_functions[i].name != nullptr; i++) {
		if (renamed_functions[i].name == p_name) {
			return renamed_functions[i].replacement;
		}
	}
	return {};
}

bool ShaderDeprecatedConverter::has_removed_render_mode(RS::ShaderMode p_mode, const String &p_name) {
	for (int i = 0; removed_render_modes[i].name != nullptr; i++) {
		if (removed_render_modes[i].mode == p_mode && removed_render_modes[i].name == p_name) {
			return true;
		}
	}
	return false;
}

bool ShaderDeprecatedConverter::can_remove_render_mode(const String &p_name) {
	for (int i = 0; removed_render_modes[i].name != nullptr; i++) {
		if (removed_render_modes[i].name == p_name) {
			return removed_render_modes[i].can_remove;
		}
	}
	return false;
}

bool ShaderDeprecatedConverter::has_removed_type(const String &p_name) {
	for (int i = 0; removed_types[i] != nullptr; i++) {
		if (removed_types[i] == p_name) {
			return true;
		}
	}
	return false;
}

static constexpr const char *token_to_str[] = {
	"", // TK_EMPTY
	"", // TK_IDENTIFIER
	"true",
	"false",
	"", // TK_FLOAT_CONSTANT
	"", // TK_INT_CONSTANT
	"", // TK_UINT_CONSTANT
	"void",
	"bool",
	"bvec2",
	"bvec3",
	"bvec4",
	"int",
	"ivec2",
	"ivec3",
	"ivec4",
	"uint",
	"uvec2",
	"uvec3",
	"uvec4",
	"float",
	"vec2",
	"vec3",
	"vec4",
	"mat2",
	"mat3",
	"mat4",
	"sampler2D",
	"isampler2D",
	"usampler2D",
	"sampler2DArray",
	"isampler2DArray",
	"usampler2DArray",
	"sampler3D",
	"isampler3D",
	"usampler3D",
	"samplerCube",
	"samplerCubeArray",
	"flat",
	"smooth",
	"const",
	"struct",
	"lowp",
	"mediump",
	"highp",
	"==",
	"!=",
	"<",
	"<=",
	">",
	">=",
	"&&",
	"||",
	"!",
	"+",
	"-",
	"*",
	"/",
	"%",
	"<<",
	">>",
	"=",
	"+=",
	"-=",
	"*=",
	"/=",
	"%=",
	"<<=",
	">>=",
	"&=",
	"|=",
	"^=",
	"&",
	"|",
	"^",
	"~",
	"++",
	"--",
	"if",
	"else",
	"for",
	"while",
	"do",
	"switch",
	"case",
	"default",
	"break",
	"continue",
	"return",
	"discard",
	"[",
	"]",
	"{",
	"}",
	"(",
	")",
	"?",
	",",
	":",
	";",
	".",
	"uniform",
	"group_uniforms",
	"instance",
	"global",
	"varying",
	"in",
	"out",
	"inout",
	"render_mode",
	"hint_default_white",
	"hint_default_black",
	"hint_default_transparent",
	"hint_normal",
	"hint_roughness_normal",
	"hint_roughness_r",
	"hint_roughness_g",
	"hint_roughness_b",
	"hint_roughness_a",
	"hint_roughness_gray",
	"hint_anisotropy",
	"source_color",
	"hint_range",
	"instance_index",
	"hint_screen_texture",
	"hint_normal_roughness_texture",
	"hint_depth_texture",
	"filter_nearest",
	"filter_linear",
	"filter_nearest_mipmap",
	"filter_linear_mipmap",
	"filter_nearest_mipmap_anisotropic",
	"filter_linear_mipmap_anisotropic",
	"repeat_enable",
	"repeat_disable",
	"shader_type",
	"", // TK_CURSOR
	"", // TK_ERROR
	"", // TK_EOF
	"\t",
	"\r",
	" ",
	"\n",
	"", // TK_BLOCK_COMMENT
	"", // TK_LINE_COMMENT
	"", // TK_PREPROC_DIRECTIVE
};
static_assert(ShaderLanguage::TK_MAX == sizeof(token_to_str) / sizeof(token_to_str[0]), "token_to_str length does not match token count (Did TK_MAX change?)");

bool ShaderDeprecatedConverter::token_is_skippable(const Token &tk) {
	switch (tk.type) {
		case ShaderLanguage::TK_TAB:
		case ShaderLanguage::TK_CR:
		case ShaderLanguage::TK_SPACE:
		case ShaderLanguage::TK_NEWLINE:
		case ShaderLanguage::TK_BLOCK_COMMENT:
		case ShaderLanguage::TK_LINE_COMMENT:
		case ShaderLanguage::TK_PREPROC_DIRECTIVE:
			return true;
		default:
			break;
	}
	return false;
};

List<SL::Token>::Element *ShaderDeprecatedConverter::_get_next_token_ptr(List<Token>::Element *_curr_ptr) const {
	ERR_FAIL_COND_V(_curr_ptr == nullptr, _curr_ptr);
	if (_curr_ptr->next() == nullptr) {
		return _curr_ptr;
	}
	_curr_ptr = _curr_ptr->next();
	while (token_is_skippable(_curr_ptr->get())) {
		if (_curr_ptr->next() == nullptr) {
			return _curr_ptr;
		}
		_curr_ptr = _curr_ptr->next();
	}
	return _curr_ptr;
};

List<SL::Token>::Element *ShaderDeprecatedConverter::_get_prev_token_ptr(List<Token>::Element *_curr_ptr) const {
	ERR_FAIL_COND_V(_curr_ptr == nullptr, _curr_ptr);
	if (_curr_ptr->prev() == nullptr) {
		return _curr_ptr;
	}
	_curr_ptr = _curr_ptr->prev();
	while (token_is_skippable(_curr_ptr->get())) {
		if (_curr_ptr->prev() == nullptr) {
			return _curr_ptr;
		}
		_curr_ptr = _curr_ptr->prev();
	}
	return _curr_ptr;
};

List<SL::Token>::Element *ShaderDeprecatedConverter::get_next_token() {
	curr_ptr = _get_next_token_ptr(curr_ptr);
	return curr_ptr;
};

List<SL::Token>::Element *ShaderDeprecatedConverter::get_prev_token() {
	curr_ptr = _get_prev_token_ptr(curr_ptr);
	return curr_ptr;
};

List<SL::Token>::Element *ShaderDeprecatedConverter::remove_cur_and_get_next() {
	ERR_FAIL_COND_V(!curr_ptr, nullptr);
	List<Token>::Element *prev = curr_ptr->prev();
	if (!prev) {
		prev = curr_ptr->next();
		code_tokens.erase(curr_ptr);
		while (token_is_skippable(prev->get())) {
			if (prev->next() == nullptr) {
				return prev;
			}
			prev = prev->next();
		}
		return prev;
	}
	code_tokens.erase(curr_ptr);
	curr_ptr = prev;
	return get_next_token();
};

SL::TokenType ShaderDeprecatedConverter::_peek_tk_type(int64_t count, List<Token>::Element **r_pos) const {
	ERR_FAIL_COND_V(!curr_ptr, ShaderLanguage::TK_EOF);
	if (count == 0) {
		return curr_ptr->get().type;
	}

	bool backwards = count < 0;
	uint64_t max_count = abs(count);
	auto start_ptr = curr_ptr;
	for (uint64_t i = 0; i < max_count; i++) {
		auto _ptr = backwards ? _get_prev_token_ptr(start_ptr) : _get_next_token_ptr(start_ptr);
		if (!_ptr) {
			if (r_pos) {
				*r_pos = start_ptr;
			}
			return ShaderLanguage::TK_EOF;
		}
		start_ptr = _ptr;
	}
	if (r_pos) {
		*r_pos = start_ptr;
	}
	return start_ptr->get().type;
}

bool ShaderDeprecatedConverter::scope_has_decl(const String &p_scope, const String &p_name) const {
	if (uniform_decls.has(p_name) || function_decls.has(p_name) ||
			(scope_declarations.has("<global>") && scope_declarations["<global>"].has(p_name)) ||
			(scope_declarations.has(p_scope) && scope_declarations[p_scope].has(p_name))) {
		return true;
	}
	return false;
}

SL::TokenType ShaderDeprecatedConverter::peek_next_tk_type(uint32_t count) const {
	return _peek_tk_type(count);
};

SL::TokenType ShaderDeprecatedConverter::peek_prev_tk_type(uint32_t count) const {
	return _peek_tk_type(-((int64_t)count));
};

List<SL::Token>::Element *ShaderDeprecatedConverter::get_pos() const {
	ERR_FAIL_COND_V(!curr_ptr, nullptr);
	return curr_ptr;
};

bool ShaderDeprecatedConverter::reset_to(List<Token>::Element *p_pos) {
	ERR_FAIL_COND_V(p_pos == nullptr, false);
	curr_ptr = p_pos;
	return true;
};

bool ShaderDeprecatedConverter::insert_after(const Vector<Token> &token_list, List<Token>::Element *p_pos) {
	ERR_FAIL_COND_V(p_pos == nullptr, false);
	for (int i = token_list.size() - 1; i >= 0; i--) {
		const Token &tk = token_list[i];
		code_tokens.insert_after(p_pos, { tk.type, tk.text, tk.constant, tk.line, tk.length, NEW_IDENT });
	}
	return true;
};

bool ShaderDeprecatedConverter::insert_before(const Vector<Token> &token_list, List<Token>::Element *p_pos) {
	ERR_FAIL_COND_V(p_pos == nullptr, false);
	for (const Token &tk : token_list) {
		code_tokens.insert_before(p_pos, { tk.type, tk.text, tk.constant, tk.line, tk.length, NEW_IDENT });
	}
	return true;
};

bool ShaderDeprecatedConverter::insert_after(const Token &token, List<Token>::Element *p_pos) {
	ERR_FAIL_COND_V(p_pos == nullptr, false);
	Token new_token = token;
	new_token.pos = NEW_IDENT;
	code_tokens.insert_after(p_pos, new_token);
	return true;
};

bool ShaderDeprecatedConverter::insert_before(const Token &token, List<Token>::Element *p_pos) {
	ERR_FAIL_COND_V(p_pos == nullptr, false);
	Token new_token = token;
	new_token.pos = NEW_IDENT;
	code_tokens.insert_before(p_pos, new_token);
	return true;
};

List<SL::Token>::Element *ShaderDeprecatedConverter::replace_curr(const Token &token) {
	ERR_FAIL_COND_V(curr_ptr == nullptr, nullptr);
	Token new_token = token;
	new_token.pos = NEW_IDENT;
	List<Token>::Element *prev = curr_ptr;
	curr_ptr = code_tokens.insert_before(curr_ptr, new_token);
	code_tokens.erase(prev);
	return curr_ptr;
};

SL::Token ShaderDeprecatedConverter::mkTok(TokenType p_type, const StringName &p_text, double constant, uint16_t p_line) {
	return { p_type, p_text, constant, p_line, 0, NEW_IDENT };
};

bool ShaderDeprecatedConverter::_insert_uniform_declaration(const String &p_name) {
	if (after_type_decl == nullptr) {
		return false;
	}
	TokenType type = get_removed_builtin_type(p_name);
	Vector<TokenType> hints = get_removed_builtin_hints(p_name);
	Vector<Token> uni_decl = { mkTok(TT::TK_NEWLINE), mkTok(TT::TK_UNIFORM), mkTok(TT::TK_SPACE), mkTok(type),
		mkTok(TT::TK_SPACE), mkTok(TT::TK_IDENTIFIER, p_name), mkTok(TT::TK_SPACE), mkTok(TT::TK_COLON),
		mkTok(TT::TK_SPACE) };
	for (int i = 0; i < hints.size(); i++) {
		uni_decl.append(mkTok(hints[i]));
		if (i < hints.size() - 1) {
			uni_decl.append(mkTok(TT::TK_COMMA));
			uni_decl.append(mkTok(TT::TK_SPACE));
		}
	}
	uni_decl.append_array({ mkTok(TT::TK_SEMICOLON), mkTok(TT::TK_NEWLINE) });
	bool ret = insert_after(uni_decl, after_type_decl);
	if (!ret) {
		return false;
	}
	auto cur_pos = get_pos();
	reset_to(after_type_decl);
	UniformDecl uni;
	uni.start_pos = get_next_token(); // uniform
	uni.type_pos = get_next_token(); // type
	uni.name_pos = get_next_token(); // id
	get_next_token(); // colon
	for (int i = 0; i < hints.size(); i++) {
		uni.hint_poses.push_back(get_next_token()); // hint
		if (i < hints.size() - 1) {
			get_next_token(); // comma
		}
	}
	uni.end_pos = get_next_token();
	uniform_decls[p_name] = uni;
	reset_to(cur_pos);
	return true;
}

RS::ShaderMode ShaderDeprecatedConverter::get_shader_mode_from_string(const String &p_mode) {
	if (p_mode == "spatial") {
		return RS::SHADER_SPATIAL;
	} else if (p_mode == "canvas_item") {
		return RS::SHADER_CANVAS_ITEM;
	} else if (p_mode == "particles") {
		return RS::SHADER_PARTICLES;
	} else { // 3.x didn't support anything else.
		return RS::SHADER_MAX;
	}
}
// Remove from the current token to end (exclsusive) and return the new current token.
List<SL::Token>::Element *ShaderDeprecatedConverter::_remove_from_curr_to(List<Token>::Element *p_end) {
	ERR_FAIL_COND_V(p_end == nullptr, nullptr);
	while (curr_ptr != p_end) {
		auto next = curr_ptr->next();
		code_tokens.erase(curr_ptr);
		curr_ptr = next;
	}
	return curr_ptr;
}

String ShaderDeprecatedConverter::get_tokentype_text(TokenType tk_type) {
	return token_to_str[tk_type];
}

List<SL::Token>::Element *ShaderDeprecatedConverter::_get_end_of_closure() {
	int additional_closures = 0;
	List<Token>::Element *ptr = curr_ptr;
	bool start_is_scope_start = false;
	switch (ptr->get().type) {
		case TT::TK_CURLY_BRACKET_OPEN:
		case TT::TK_PARENTHESIS_OPEN:
		case TT::TK_BRACKET_OPEN:
			start_is_scope_start = true;
			break;
		default:
			break;
	}
	for (; ptr; ptr = ptr->next()) {
		switch (ptr->get().type) {
			case TT::TK_CURLY_BRACKET_OPEN:
			case TT::TK_PARENTHESIS_OPEN:
			case TT::TK_BRACKET_OPEN: {
				additional_closures++;
			} break;
			case TT::TK_CURLY_BRACKET_CLOSE:
			case TT::TK_PARENTHESIS_CLOSE:
			case TT::TK_BRACKET_CLOSE: {
				if (additional_closures > 0) {
					additional_closures--;
					if (start_is_scope_start && additional_closures == 0) {
						return ptr;
					}
				} else {
					return ptr;
				}
			} break;
			case TT::TK_SEMICOLON:
			case TT::TK_COMMA: {
				if (additional_closures <= 0) {
					return _get_prev_token_ptr(ptr);
				}
			} break;
			case TT::TK_EOF:
			case TT::TK_ERROR: {
				return ptr;
			} break;
			default:
				break;
		}
	}
	return ptr;
}

bool ShaderDeprecatedConverter::token_is_type(const Token &tk) {
	return (ShaderLanguage::is_token_datatype(tk.type)) || (tk.type == TT::TK_IDENTIFIER && has_removed_type(tk.text));
}

bool ShaderDeprecatedConverter::token_is_hint(const Token &tk) {
	if (tk.type == TT::TK_IDENTIFIER) {
		return has_hint_replacement(tk.text);
	}
	return SL::is_token_hint(tk.type);
}

String ShaderDeprecatedConverter::get_token_literal_text(const Token &tk) const {
	switch (tk.type) {
		case TT::TK_PREPROC_DIRECTIVE:
		case TT::TK_LINE_COMMENT:
		case TT::TK_BLOCK_COMMENT:
		case TT::TK_IDENTIFIER: { // Identifiers prefixed with `__` are modified to `_dup_` by the SL parser
			if (tk.pos == NEW_IDENT) {
				return tk.text;
			} else {
				return old_code.substr(tk.pos, tk.length);
			}
		} break;
		case TT::TK_INT_CONSTANT:
		case TT::TK_FLOAT_CONSTANT:
		case TT::TK_UINT_CONSTANT: {
			if (tk.pos == NEW_IDENT) {
				// Fix for 3.x float constants not having a decimal point.
				if (!tk.is_integer_constant() && tk.text != "") {
					return tk.text;
				}
				String const_str = rtos(tk.constant);
				if (!tk.is_integer_constant() && !const_str.contains(".")) {
					const_str += ".0";
				}
				return const_str;
			} else {
				return old_code.substr(tk.pos, tk.length);
			}
		} break;
		case TT::TK_ERROR:
		case TT::TK_EOF: {
			return "";
		} break;
		default:
			break;
	}
	return token_to_str[tk.type];
}

bool ShaderDeprecatedConverter::tokentype_is_identifier(const TokenType &tk_type) {
	return tk_type == TT::TK_IDENTIFIER || tokentype_is_new_reserved_keyword(tk_type);
}

bool ShaderDeprecatedConverter::tokentype_is_new_type(const TokenType &p_type) {
	// the following types are in both 3.x and 4.x
	return (!(
				   p_type == TT::TK_TYPE_VOID ||
				   p_type == TT::TK_TYPE_BOOL ||
				   p_type == TT::TK_TYPE_BVEC2 ||
				   p_type == TT::TK_TYPE_BVEC3 ||
				   p_type == TT::TK_TYPE_BVEC4 ||
				   p_type == TT::TK_TYPE_INT ||
				   p_type == TT::TK_TYPE_IVEC2 ||
				   p_type == TT::TK_TYPE_IVEC3 ||
				   p_type == TT::TK_TYPE_IVEC4 ||
				   p_type == TT::TK_TYPE_UINT ||
				   p_type == TT::TK_TYPE_UVEC2 ||
				   p_type == TT::TK_TYPE_UVEC3 ||
				   p_type == TT::TK_TYPE_UVEC4 ||
				   p_type == TT::TK_TYPE_FLOAT ||
				   p_type == TT::TK_TYPE_VEC2 ||
				   p_type == TT::TK_TYPE_VEC3 ||
				   p_type == TT::TK_TYPE_VEC4 ||
				   p_type == TT::TK_TYPE_MAT2 ||
				   p_type == TT::TK_TYPE_MAT3 ||
				   p_type == TT::TK_TYPE_MAT4 ||
				   p_type == TT::TK_TYPE_SAMPLER2D ||
				   p_type == TT::TK_TYPE_ISAMPLER2D ||
				   p_type == TT::TK_TYPE_USAMPLER2D ||
				   p_type == TT::TK_TYPE_SAMPLER2DARRAY ||
				   p_type == TT::TK_TYPE_ISAMPLER2DARRAY ||
				   p_type == TT::TK_TYPE_USAMPLER2DARRAY ||
				   p_type == TT::TK_TYPE_SAMPLER3D ||
				   p_type == TT::TK_TYPE_ISAMPLER3D ||
				   p_type == TT::TK_TYPE_USAMPLER3D ||
				   p_type == TT::TK_TYPE_SAMPLERCUBE)) &&
			SL::is_token_datatype(p_type);
}

// checks for reserved keywords only found in 4.x
bool ShaderDeprecatedConverter::tokentype_is_new_reserved_keyword(const TokenType &tk_type) {
	switch (tk_type) {
		// The following keyword tokens are in both 3.x and 4.x.
		case TT::TK_ARG_IN:
		case TT::TK_ARG_INOUT:
		case TT::TK_ARG_OUT:
		case TT::TK_CF_BREAK:
		case TT::TK_CF_CASE:
		case TT::TK_CF_CONTINUE:
		case TT::TK_CF_DEFAULT:
		case TT::TK_CF_DISCARD:
		case TT::TK_CF_DO:
		case TT::TK_CF_ELSE:
		case TT::TK_CF_FOR:
		case TT::TK_CF_IF:
		case TT::TK_CF_RETURN:
		case TT::TK_CF_SWITCH:
		case TT::TK_CF_WHILE:
		case TT::TK_CONST:
		case TT::TK_ERROR:
		case TT::TK_FALSE:
		case TT::TK_HINT_NORMAL_TEXTURE:
		case TT::TK_HINT_RANGE:
		case TT::TK_INTERPOLATION_FLAT:
		case TT::TK_INTERPOLATION_SMOOTH:
		case TT::TK_PRECISION_HIGH:
		case TT::TK_PRECISION_LOW:
		case TT::TK_PRECISION_MID:
		case TT::TK_RENDER_MODE:
		case TT::TK_SHADER_TYPE:
		case TT::TK_STRUCT:
		case TT::TK_TRUE:
		case TT::TK_TYPE_BOOL:
		case TT::TK_TYPE_BVEC2:
		case TT::TK_TYPE_BVEC3:
		case TT::TK_TYPE_BVEC4:
		case TT::TK_TYPE_FLOAT:
		case TT::TK_TYPE_INT:
		case TT::TK_TYPE_ISAMPLER2D:
		case TT::TK_TYPE_ISAMPLER2DARRAY:
		case TT::TK_TYPE_ISAMPLER3D:
		case TT::TK_TYPE_IVEC2:
		case TT::TK_TYPE_IVEC3:
		case TT::TK_TYPE_IVEC4:
		case TT::TK_TYPE_MAT2:
		case TT::TK_TYPE_MAT3:
		case TT::TK_TYPE_MAT4:
		case TT::TK_TYPE_SAMPLER2D:
		case TT::TK_TYPE_SAMPLER2DARRAY:
		case TT::TK_TYPE_SAMPLER3D:
		case TT::TK_TYPE_SAMPLERCUBE:
		case TT::TK_TYPE_UINT:
		case TT::TK_TYPE_USAMPLER2D:
		case TT::TK_TYPE_USAMPLER2DARRAY:
		case TT::TK_TYPE_USAMPLER3D:
		case TT::TK_TYPE_UVEC2:
		case TT::TK_TYPE_UVEC3:
		case TT::TK_TYPE_UVEC4:
		case TT::TK_TYPE_VEC2:
		case TT::TK_TYPE_VEC3:
		case TT::TK_TYPE_VEC4:
		case TT::TK_TYPE_VOID:
		case TT::TK_UNIFORM:
		case TT::TK_VARYING:
			return false;
		default:
			break;
	}
	return SL::is_token_keyword(tk_type);
}

bool ShaderDeprecatedConverter::tokentype_is_new_hint(const TokenType &tk_type) {
	switch (tk_type) {
		case TT::TK_HINT_NORMAL_TEXTURE: // These two are in both 3.x and 4.x.
		case TT::TK_HINT_RANGE:
			return false;
		default:
			break;
	}
	return SL::is_token_hint(tk_type);
}

bool ShaderDeprecatedConverter::token_is_new_builtin_func(const Token &tk) {
	return new_builtin_funcs.has(tk.text);
}

bool ShaderDeprecatedConverter::_add_comment_before(const String &p_comment, List<Token>::Element *p_pos) {
	// peek back until we hit a newline or the start of the file (EOF)
	auto start_pos = p_pos ? p_pos : get_pos();
	if (!start_pos) {
		return false;
	}
	while (start_pos->prev() && start_pos->get().type != TT::TK_NEWLINE && start_pos->get().type != TT::TK_EOF) {
		start_pos = start_pos->prev();
	}
	String comment = "/* !convert: " + p_comment + " */";
	// check if the token before this is a block comment and has the same comment
	if (start_pos->prev() && start_pos->prev()->get().type == TT::TK_BLOCK_COMMENT && get_token_literal_text(start_pos->prev()->get()) == comment) {
		return true;
	}

	return insert_after({ mkTok(TT::TK_BLOCK_COMMENT, comment), mkTok(TT::TK_NEWLINE) }, start_pos);
}

bool ShaderDeprecatedConverter::_add_comment_at_eol(const String &p_comment, List<Token>::Element *p_pos) {
	// Peek forward until we hit a newline or the end of the file (EOF)
	auto start_pos = p_pos ? p_pos : get_pos();
	if (!start_pos) {
		return false;
	}
	while (start_pos->get().type != TT::TK_NEWLINE && start_pos->get().type != TT::TK_EOF) {
		start_pos = start_pos->next();
	}
	String comment = "/* !convert: " + p_comment + " */";
	if (start_pos->prev() && start_pos->prev()->get().type == TT::TK_BLOCK_COMMENT && get_token_literal_text(start_pos->prev()->get()) == comment) {
		return true;
	}
	return insert_before(mkTok(TT::TK_BLOCK_COMMENT, comment), start_pos);
}

void ShaderDeprecatedConverter::reset() {
	ShaderLanguage sl;
	code_tokens.clear();
	sl.token_debug_stream(old_code, code_tokens, true);
	code_tokens.push_back(eof_token);
	code_tokens.push_front(eof_token);
	uniform_decls.clear();
	var_decls.clear();
	function_decls.clear();
	curr_ptr = code_tokens.front();
}

#define COND_MSG_FAIL(cond, msg) \
	if (unlikely(cond)) {        \
		err_str = msg;           \
		return false;            \
	}
#define COND_LINE_MSG_FAIL(cond, line, msg) \
	if (unlikely(cond)) {                   \
		err_line = line + 1;                \
		err_str = msg;                      \
		return false;                       \
	}
#define LINE_MSG_FAIL(line, msg) \
	err_line = line + 1;         \
	err_str = msg;               \
	return false;
#define MSG_FAIL(msg)    \
	err_line = line + 1; \
	err_str = msg;       \
	return false;
#define INSERT_FAIL(insert_ret)                          \
	if (unlikely(!insert_ret)) {                         \
		err_str = "Internal error: Token insert failed"; \
		return false;                                    \
	}

#define EOF_FAIL(tokE)                                            \
	COND_MSG_FAIL(tokE == nullptr, RTR("Unexpected end of file")) \
	COND_LINE_MSG_FAIL(tokE->get().type == TT::TK_EOF || tokE->get().type == TT::TK_ERROR, tokE->get().line, tokE->get().type == TT::TK_ERROR ? vformat(RTR("Parser Error (%s) ", tokE->get().text)) : RTR("Unexpected end of file"))

bool ShaderDeprecatedConverter::_skip_struct() {
	TokE *struct_name = get_next_token();
	EOF_FAIL(struct_name);
	TokE *struct_body_start;
	if (struct_name->get().type == TT::TK_CURLY_BRACKET_OPEN) {
		struct_body_start = struct_name;
	} else {
		struct_body_start = get_next_token();
	}
	EOF_FAIL(struct_body_start);
	COND_LINE_MSG_FAIL(struct_body_start->get().type != TT::TK_CURLY_BRACKET_OPEN, struct_body_start->get().line, RTR("Expected '{' after struct declaration"));
	TokE *struct_body_end = _get_end_of_closure();
	EOF_FAIL(struct_body_end);
	COND_LINE_MSG_FAIL(struct_body_end->get().type != TT::TK_CURLY_BRACKET_CLOSE, struct_body_start->get().line, RTR("Expected '}' bracket"));
	reset_to(struct_body_end);
	if (tokentype_is_identifier(peek_next_tk_type())) {
		get_next_token();
	}
	return true;
}

bool ShaderDeprecatedConverter::preprocess_code() {
	reset();
	COND_MSG_FAIL(code_tokens.size() == 0, RTR("Empty shader file"));
	StringName mode_string;
	{
		COND_MSG_FAIL(code_tokens.size() < 3, RTR("Invalid shader file"));
		auto first_token = get_next_token();
		EOF_FAIL(first_token);
		COND_LINE_MSG_FAIL(first_token->get().type != TT::TK_SHADER_TYPE, first_token->get().line, RTR("Shader type must be first token"));
		auto id_token = get_next_token();
		EOF_FAIL(id_token);
		COND_LINE_MSG_FAIL(id_token->get().type != TT::TK_IDENTIFIER, id_token->get().line, RTR("Invalid shader type"));
		mode_string = id_token->get().text;
		auto token = get_next_token();
		EOF_FAIL(token);
		COND_LINE_MSG_FAIL(token->get().type != TT::TK_SEMICOLON, token->get().line, RTR("Expected semi-colon after shader type"));
		shader_mode = get_shader_mode_from_string(mode_string);
	}
	after_type_decl = get_pos();

	/***
	 * The first pass gets the uniform declarations; we require this is to ensure idempotency for inserting new uniforms and replacing type hints.
	 * The second pass gets the function declarations; these are used for determining if a renamed built-in is valid in the current scope
	 * The third pass gets the variable declarations; These are used for determining if renamed built-ins have been previously declared, and for detecting new keywords used as identifiers.
	 */
	// first pass; we just get the Uniform declarations
	auto skip_array_size = [&]() -> bool {
		TokE *next_tk = get_pos();
		DEV_ASSERT(next_tk->get().type == TT::TK_BRACKET_OPEN);
		next_tk = _get_end_of_closure();
		EOF_FAIL(next_tk);
		COND_LINE_MSG_FAIL(next_tk->get().type != TT::TK_BRACKET_CLOSE, next_tk->get().line, RTR("Expected ']' after array type"));
		reset_to(next_tk); // skip the array size
		next_tk = get_next_token();
		EOF_FAIL(next_tk);
		return true;
	};
	Vector<TokE *> uniform_type_poses;

	auto first_pass_func = [&]() {
		while (true) {
			auto cur_tok = get_next_token();
			if (cur_tok->get().type == TT::TK_EOF) {
				break;
			}
			switch (cur_tok->get().type) {
				case TT::TK_UNIFORM: {
					UniformDecl uni;
					uni.start_pos = cur_tok;
					auto next_tk = get_next_token();
					EOF_FAIL(next_tk);
					while (SL::is_token_precision(next_tk->get().type) || SL::is_token_interpolation(next_tk->get().type)) {
						next_tk = get_next_token();
						EOF_FAIL(next_tk);
					}
					COND_LINE_MSG_FAIL(!token_is_type(next_tk->get()), next_tk->get().line, RTR("Expected type after 'uniform'"));
					uni.type_pos = next_tk;
					uniform_type_poses.push_back(next_tk);
					next_tk = get_next_token();
					EOF_FAIL(next_tk);
					if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
						uni.is_array = true;
						if (!skip_array_size())
							return false;
						next_tk = get_pos();
					}
					COND_LINE_MSG_FAIL(!tokentype_is_identifier(next_tk->get().type), next_tk->get().line, RTR("Expected identifier after uniform type"));
					String name = get_token_literal_text(next_tk->get());
					uni.name_pos = next_tk;
					next_tk = get_next_token();
					EOF_FAIL(next_tk);
					if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
						uni.is_array = true;
						if (!skip_array_size())
							return false;
						next_tk = get_pos();
					}
					if (next_tk->get().type == TT::TK_COLON) {
						while (true) {
							next_tk = get_next_token();
							EOF_FAIL(next_tk);
							COND_LINE_MSG_FAIL(!token_is_hint(next_tk->get()), next_tk->get().line, RTR("Expected hint after ':' in uniform declaration"));
							uni.hint_poses.push_back(next_tk);
							next_tk = get_next_token();
							EOF_FAIL(next_tk);
							if (next_tk->get().type == TT::TK_PARENTHESIS_OPEN) {
								next_tk = _get_end_of_closure();
								EOF_FAIL(next_tk);
								COND_LINE_MSG_FAIL(next_tk->get().type != TT::TK_PARENTHESIS_CLOSE, next_tk->get().line, RTR("Expected ')' after hint range"));
								reset_to(next_tk); // skip the hint range
								next_tk = get_next_token();
								EOF_FAIL(next_tk);
							}
							if (next_tk->get().type != TT::TK_COMMA) {
								break;
							}
						}
					}
					if (next_tk->get().type == TT::TK_OP_ASSIGN) {
						next_tk = _get_end_of_closure();
						EOF_FAIL(next_tk);
						reset_to(next_tk); // skip the assignment
						next_tk = get_next_token();
					}
					uni.end_pos = next_tk;
					EOF_FAIL(uni.end_pos);
					COND_LINE_MSG_FAIL(uni.end_pos->get().type != TT::TK_SEMICOLON, uni.end_pos->get().line, RTR("Expected ';' after uniform declaration"));
					uniform_decls[name] = uni;
				} break;
				default:
					break;
			}
		}
		return true;
	};

	// Past the start and type tokens, at id or bracket open token
	auto process_decl_statement = [&](TokE *start_tok, TokE *type_tok, String scope = "<global>", bool func_args = false) -> bool {
		while (true) {
			EOF_FAIL(start_tok);
			EOF_FAIL(type_tok);
			TokE *next_tk = get_pos();
			COND_LINE_MSG_FAIL(!token_is_type(type_tok->get()), type_tok->get().line, RTR("Expected type in declaration"));
			VarDecl var;
			var.start_pos = start_tok;
			var.type_pos = type_tok;
			var.is_func_arg = func_args;
			EOF_FAIL(next_tk);
			if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
				var.is_array = true;
				var.new_arr_style_decl = true;
				if (!skip_array_size())
					return false;
				next_tk = get_pos();
			}
			COND_LINE_MSG_FAIL(!tokentype_is_identifier(next_tk->get().type), next_tk->get().line, RTR("Expected identifier after type in declaration"));
			var.name_pos = next_tk;
			String name = get_token_literal_text(var.name_pos->get());
			next_tk = get_next_token();
			EOF_FAIL(next_tk);
			TokE *end_pos = next_tk;
			if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
				var.is_array = true;
				if (!skip_array_size())
					return false;
				end_pos = get_pos();
				next_tk = end_pos;
			}
			if (next_tk->get().type == TT::TK_OP_ASSIGN) {
				end_pos = _get_end_of_closure();
				EOF_FAIL(end_pos);
				reset_to(end_pos); // Skip the assignment.
				if (end_pos->get().type == TT::TK_PARENTHESIS_CLOSE && func_args) {
					next_tk = end_pos;
					end_pos = end_pos->prev(); // Including whitespace before parenthesis.
				} else {
					next_tk = get_next_token();
					EOF_FAIL(next_tk);
					end_pos = next_tk;
				}
			}
			var.end_pos = end_pos;
			COND_LINE_MSG_FAIL(!(next_tk->get().type == TT::TK_SEMICOLON ||
									   next_tk->get().type == TT::TK_COMMA ||
									   next_tk->get().type == TT::TK_PARENTHESIS_CLOSE),
					next_tk->get().line, RTR("Expected comma or semi-colon after variable declaration"));
			if (var_decls.has(name)) {
				var_decls[name].push_back(var);
			} else {
				var_decls[name] = { var };
			}
			if (!scope_declarations.has(scope)) {
				scope_declarations[scope] = HashSet<String>();
			}
			scope_declarations[scope].insert(name);
			if (next_tk->get().type == TT::TK_COMMA) {
				next_tk = get_next_token();
				EOF_FAIL(next_tk);
				start_tok = next_tk;
				if (func_args) {
					while (next_tk->get().type == TT::TK_CONST ||
							SL::is_token_precision(next_tk->get().type) ||
							SL::is_token_arg_qual(next_tk->get().type) ||
							SL::is_token_interpolation(next_tk->get().type)) {
						next_tk = get_next_token();
						EOF_FAIL(next_tk);
					}
					type_tok = next_tk; // next_tk is type
					COND_LINE_MSG_FAIL(!token_is_type(type_tok->get()), type_tok->get().line, RTR("Expected type after comma in function argument declaration"));
					next_tk = get_next_token(); // id
					EOF_FAIL(next_tk);
				} // otherwise, this is a compound declaration, leave type_tok as is
			} else if (next_tk->get().type == TT::TK_PARENTHESIS_CLOSE) {
				break;
			} else if (next_tk->get().type == TT::TK_SEMICOLON) {
				break;
			}
		}
		return true;
	};

	// Past the start and type tokens, at id or bracket open token.
	auto process_func_decl_statement = [&](TokE *start_tok, TokE *type_tok, bool second_pass) -> bool {
		FunctionDecl func;
		func.start_pos = start_tok; // type or const
		func.type_pos = type_tok; // type
		TokE *next_tk = get_pos(); // id or array size
		if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
			func.has_array_return_type = true;
			if (!skip_array_size())
				return false;
			next_tk = get_pos();
		}
		func.name_pos = next_tk; // id
		String name = get_token_literal_text(func.name_pos->get());
		func.args_start_pos = get_next_token(); // paren
		EOF_FAIL(func.args_start_pos);
		if (peek_next_tk_type() == TT::TK_PARENTHESIS_CLOSE) {
			func.args_end_pos = get_next_token();
		} else { // args are present
			func.args_end_pos = _get_end_of_closure();
			EOF_FAIL(func.args_end_pos);
			if (second_pass) { // second_pass == true means we're only getting the function declarations
				// skip the args
				reset_to(func.args_end_pos);
			} else {
				TokE *start_pos = get_next_token();
				TokE *type_pos = start_pos;
				while (type_pos->get().type == TT::TK_CONST || SL::is_token_precision(type_pos->get().type) || SL::is_token_arg_qual(type_pos->get().type) || SL::is_token_interpolation(type_pos->get().type)) {
					type_pos = get_next_token();
					EOF_FAIL(type_pos);
				}
				get_next_token(); // id
				if (!process_decl_statement(start_pos, type_pos, name, true)) {
					return false;
				}
			}
		}
		// Currently at paren close.
		func.body_start_pos = get_next_token(); // curly open
		EOF_FAIL(func.body_start_pos);
		COND_LINE_MSG_FAIL(func.body_start_pos->get().type != TT::TK_CURLY_BRACKET_OPEN, func.body_start_pos->get().line, RTR("Expected '{' after function declaration"));
		func.body_end_pos = _get_end_of_closure();
		EOF_FAIL(func.body_end_pos);
		COND_LINE_MSG_FAIL(func.body_end_pos->get().type != TT::TK_CURLY_BRACKET_CLOSE, func.body_start_pos->get().line, RTR("Expected '}' bracket"));
		if (second_pass) { // second_pass == false means the functions have already been processed.
			function_decls[name] = func;
		} else {
#ifdef DEBUG_ENABLED
			if (!function_decls.has(name)) {
				LINE_MSG_FAIL(func.start_pos->get().line, vformat(RTR("Function declaration not found in third pass (%s)"), name));
			} else {
				// compare our values to ensure they match
				auto &first_pass = function_decls[name];
				bool matches = first_pass.start_pos == func.start_pos && first_pass.type_pos == func.type_pos && first_pass.name_pos == func.name_pos && first_pass.args_start_pos == func.args_start_pos && first_pass.args_end_pos == func.args_end_pos && first_pass.body_start_pos == func.body_start_pos && first_pass.body_end_pos == func.body_end_pos;
				COND_LINE_MSG_FAIL(!matches, func.start_pos->get().line, vformat(RTR("Function declaration mismatch in third pass (%s)"), name));
			}
#endif
		}
		return true;
	};

	// Second pass: we get only the function declarations.
	// Third pass: we get only the variable declarations.
	auto decl_pass_func = [&](bool second_pass) -> bool {
		reset_to(after_type_decl);
		String curr_func = "<global>";
		while (true) {
			TokE *cur_tok = get_next_token();
			if (cur_tok->get().type == TT::TK_EOF) {
				break;
			}

			if (!second_pass) {
				for (KeyValue<String, FunctionDecl> &E : function_decls) {
					FunctionDecl &func = E.value;
					if (cur_tok == func.body_start_pos) {
						curr_func = E.key;
					} else if (cur_tok == func.body_end_pos) {
						curr_func = "<global>";
					}
				}
			}
			if (cur_tok->get().type == TT::TK_STRUCT) {
				if (!_skip_struct()) {
					return false;
				}
				continue;
			}
			if (uniform_type_poses.has(cur_tok)) {
				continue;
			}
			if (token_is_type(cur_tok->get())) {
				bool is_decl = tokentype_is_identifier(peek_next_tk_type());
				bool is_function = peek_next_tk_type(2) == TT::TK_PARENTHESIS_OPEN;
				if (!is_decl) {
					// Check if this is an array declaration.
					TokE *next_tk = get_next_token();
					if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
						TokE *bracket_end = _get_end_of_closure();
						EOF_FAIL(bracket_end);
						COND_LINE_MSG_FAIL(bracket_end->get().type != TT::TK_BRACKET_CLOSE, bracket_end->get().line, RTR("Expected ']' after array type"));
						reset_to(bracket_end);
						TokE *next_next_tk = get_next_token();
						if (next_next_tk && next_next_tk->get().type == TT::TK_IDENTIFIER) {
							is_decl = true;
							if (peek_next_tk_type() == TT::TK_PARENTHESIS_OPEN) {
								is_function = true;
							} else {
								is_function = false;
							}
						}
					}
					reset_to(cur_tok);
				}
				if (!is_decl) {
					continue;
				}
				TokE *type_pos = cur_tok;
				TokE *start_pos = type_pos; // Start and Type
				// peek back to see if previous was const or varying
				if (peek_prev_tk_type() == TT::TK_CONST || peek_prev_tk_type() == TT::TK_VARYING) {
					start_pos = get_prev_token();
					get_next_token(); // Back to type
				}
				TokE *id_tok = get_next_token(); // id or bracket open
				EOF_FAIL(id_tok);
				if (is_function) { // function declaration
					if (!process_func_decl_statement(start_pos, type_pos, second_pass)) {
						return false;
					}
					// backup to before the curly bracket open
					get_prev_token();
				} else if (!second_pass) { // other non-uniform declaration (global const, varying, locals, etc.)
					if (!process_decl_statement(start_pos, type_pos, curr_func)) {
						return false;
					}
				}
			}
		}
		return true;
	};
	// first pass, get uniform declarations
	if (!first_pass_func()) {
		err_str = vformat(RTR("First pre-process pass failed: %s"), err_str);
		curr_ptr = code_tokens.front();
		return false;
	}

	// Second pass, get function declarations
	if (!decl_pass_func(true)) {
		function_pass_failed = true;
		err_str = vformat(RTR("Second pre-process pass failed: %s"), err_str);
		curr_ptr = code_tokens.front();
		return false;
	}
	// Third pass, get variable declarations
	if (!decl_pass_func(false)) {
		var_pass_failed = true;
		err_str = vformat(RTR("Third pre-process pass failed: %s"), err_str);
		if (assume_correct) {
			curr_ptr = code_tokens.front();
			return false;
		}
	}
	curr_ptr = code_tokens.front();
	return true;
}

int ShaderDeprecatedConverter::get_error_line() const {
	return err_line;
}

bool ShaderDeprecatedConverter::is_code_deprecated() {
	String mode_str = SL::get_shader_type(old_code);
	if (mode_str.is_empty()) {
		// If it failed, it's because it was prefixed with a preproc directive (4.x only) or it's not a shader file.
		return false;
	}
	auto mode = get_shader_mode_from_string(mode_str);
	if (mode == RS::SHADER_MAX) {
		return false;
	}

	if (!preprocess_code()) { // this will set err_str if it fails
		return false;
	}
	reset_to(after_type_decl);

	// Negative cases first, then positive cases

	// check for pre-processor directives (4.x only)
	{
		TokE *cur_tok = code_tokens.front();
		while (cur_tok) {
			if (cur_tok->get().type == TT::TK_PREPROC_DIRECTIVE) {
				return false;
			}
			cur_tok = cur_tok->next();
		}
	}

	// Check declarations for negative conditions.
	for (const auto &E : uniform_decls) {
		const UniformDecl &uni = E.value;
		if (uni.is_array) { // 3.x did not have array uniforms.
			return false;
		} else if (tokentype_is_new_type(uni.type_pos->get().type)) {
			return false;
		}
		for (const auto &hint : uni.hint_poses) {
			if (tokentype_is_new_hint(hint->get().type)) {
				return false;
			}
		}
	}

	for (const auto &E : function_decls) {
		const FunctionDecl &func = E.value;
		if (func.has_array_return_type) { // 3.x did not have array return types.
			return false;
		} else if (tokentype_is_new_type(func.type_pos->get().type)) {
			return false;
		}
	}

	for (const auto &E : var_decls) {
		for (auto &var_decl : E.value) {
			if (var_decl.is_array && var_decl.is_func_arg) { // 3.x did not allow array function arguments
				return false;
			} else if (var_decl.new_arr_style_decl) { // 3.x did not have the `float[] x` style of array declarations
				return false;
			} else if (tokentype_is_new_type(var_decl.type_pos->get().type)) {
				return false;
			}
		}
	}

	// Check token stream for negative cases.
	{
		String curr_func = "<global>";
		while (true) {
			TokE *cur_tok = get_next_token();
			DEV_ASSERT(cur_tok);
			if (cur_tok->get().type == TT::TK_EOF || cur_tok->get().type == TT::TK_ERROR) {
				break;
			}
			for (auto &E : function_decls) {
				auto &func = E.value;
				if (cur_tok == func.body_start_pos) {
					curr_func = E.key;
					break;
				} else if (cur_tok == func.body_end_pos) {
					curr_func = "<global>";
					break;
				}
			}
			if (cur_tok->get().type == TT::TK_STRUCT) {
				if (!_skip_struct()) {
					return false;
				}
				continue;
			}

			if (tokentype_is_new_type(cur_tok->get().type) && peek_next_tk_type() == TT::TK_IDENTIFIER) {
				return false;
			} else if (cur_tok->get().type == TT::TK_UNIFORM) {
				// peek the last token to check if it's TK_GLOBAL or TK_INSTANCE
				TT tp = peek_prev_tk_type();
				if (tp == TT::TK_GLOBAL || tp == TT::TK_INSTANCE) { // Added in 4.x.
					return false;
				}
			} else if (cur_tok->get().type == TT::TK_IDENTIFIER) {
				String id = get_token_literal_text(cur_tok->get());
				if (has_builtin_rename(shader_mode, id, curr_func) || is_removed_builtin(shader_mode, id, curr_func)) {
					if (scope_has_decl(curr_func, id)) {
						// The renamed identifiers are global identifiers in 3.x and can't be redefined in either the global scope or the function scope they're valid for.
						// If they were declared previously within the global or current scope, this would be a 4.x shader.
						return false;
					}
				}
			}
		}
	}

	// Check declarations for positive conditions
	for (const auto &E : uniform_decls) {
		const UniformDecl &uni = E.value;
		if (uni.type_pos->get().type == TT::TK_IDENTIFIER && has_removed_type(get_token_literal_text(uni.type_pos->get()))) { // unported 3.x type
			return true;
		} else if (tokentype_is_new_reserved_keyword(uni.name_pos->get().type)) {
			return true;
		}
		for (const auto &hint : uni.hint_poses) {
			if (hint->get().type == TT::TK_IDENTIFIER && has_hint_replacement(get_token_literal_text(hint->get()))) {
				return true;
			}
		}
	}

	for (const auto &E : function_decls) {
		const FunctionDecl &func = E.value;
		const TokenType return_type = func.type_pos->get().type;
		String name = get_token_literal_text(func.name_pos->get());
		if (func.type_pos->get().type == TT::TK_IDENTIFIER && has_removed_type(get_token_literal_text(func.type_pos->get()))) {
			return true;
		} else if (func.name_pos->get().type == TT::TK_IDENTIFIER && is_renamed_function(shader_mode, name) && return_type == get_renamed_function_type(name)) {
			return true;
		} else if (tokentype_is_new_reserved_keyword(func.name_pos->get().type)) {
			return true;
		} else if (new_builtin_funcs.has(get_token_literal_text(func.name_pos->get()))) {
			return true;
		}
	}

	for (const auto &E : var_decls) {
		for (auto &var_decl : E.value) {
			if (var_decl.type_pos->get().type == TT::TK_IDENTIFIER && has_removed_type(get_token_literal_text(var_decl.type_pos->get()))) {
				return true;
			} else if (tokentype_is_new_reserved_keyword(var_decl.name_pos->get().type)) { // Id is new reserved keyword
				return true;
			}
		}
	}

	bool is_3x = false;
	bool should_break = false;
	String curr_func = "<global>";
	reset_to(after_type_decl);
	// check token stream for positive cases
	while (true) {
		TokE *cur_tok = get_next_token();
		if (cur_tok->get().type == TT::TK_EOF || cur_tok->get().type == TT::TK_ERROR) {
			break;
		}

		for (auto &E : function_decls) {
			auto &func = E.value;
			if (cur_tok == func.body_start_pos) {
				curr_func = E.key;
				break;
			} else if (cur_tok == func.body_end_pos) {
				curr_func = "<global>";
				break;
			}
		}
		if (cur_tok->get().type == TT::TK_STRUCT) {
			if (!_skip_struct()) {
				return false;
			}
			continue;
		}

		switch (cur_tok->get().type) {
			case TT::TK_FLOAT_CONSTANT: {
				String const_str = get_token_literal_text(cur_tok->get());
				// 3.x float constants allowed for a value without a decimal point if it ended in `f` (e.g. `1f`)
				if (const_str.ends_with("f") && const_str.find(".") == -1 && const_str.find("e") == -1) {
					return true;
				}
			} break;
			case TT::TK_RENDER_MODE: {
				while (true) {
					auto next_tk = get_next_token();
					if (next_tk->get().type == TT::TK_IDENTIFIER) {
						String id_text = get_token_literal_text(next_tk->get());
						if (is_renamed_render_mode(shader_mode, id_text) || has_removed_render_mode(shader_mode, id_text)) {
							return true;
						}
					} else {
						COND_LINE_MSG_FAIL(next_tk->get().type != TT::TK_COMMA && next_tk->get().type != TT::TK_SEMICOLON, next_tk->get().line, "Invalid render mode declaration");
					}
					if (next_tk->get().type == TT::TK_SEMICOLON) {
						break;
					}
				}
			} break;
			case TT::TK_IDENTIFIER: {
				String id = get_token_literal_text(cur_tok->get());
				if (has_builtin_rename(shader_mode, id, curr_func) || is_removed_builtin(shader_mode, id, curr_func)) {
					if (!scope_has_decl(curr_func, id)) {
						is_3x = true;
						if (!var_pass_failed) {
							return true;
						}
						// Do not stop checking in this case; the third pass may have failed and the decls may be incomplete.
					}
				} else if (has_removed_type(id) && peek_next_tk_type() == TT::TK_IDENTIFIER) {
					// declaration with unported 3.x type
					return true;
				}
			} break;
			default:
				break;
		}
		if (should_break) {
			break;
		}
	}
	curr_ptr = code_tokens.front();
	return is_3x;
}

String ShaderDeprecatedConverter::get_error_text() const {
	return err_str;
}

bool ShaderDeprecatedConverter::convert_code() {
	/**
	 * We need to do the following:
	 *  * Replace everything in RenamesMap3To4::shaders_renames
	 *	* the usage of SCREEN_TEXTURE, DEPTH_TEXTURE, and NORMAL_ROUGHNESS_TEXTURE necessitates adding a uniform declaration at the top of the file
	 *	* async_visible and async_hidden render modes need to be removed
	 *	* If shader_type is "particles", need to rename the function "void vertex()" to "void process()"
	 *  * Invert all usages of CLEARCOAT_GLOSS:
	 *    * Invert all lefthand assignments:
	 * 			- `CLEARCOAT_GLOSS = 5.0 / foo;`
	 * 			becomes: `CLEARCOAT_ROUGHNESS = (1.0 - (5.0 / foo));`,
	 *          - `CLEARCOAT_GLOSS *= 1.1;`
	 * 			becomes `CLEARCOAT_ROUGHNESS = (1.0 - ((1.0 - CLEARCOAT_ROUGHNESS) * 1.1));`
	 *    * Invert all righthand usages
	 * 			- `foo = CLEARCOAT_GLOSS;`
	 * 			becomes: `foo = (1.0 - CLEARCOAT_ROUGHNESS);`
	 *  * Wrap `INDEX` in `int()` casts if necessary.
	 *	* Check for use of `specular_blinn` and `specular_phong` render modes; not supported in 4.x, throw an error.
	 *	* Check for use of `MODULATE`; not supported in 4.x, throw an error.
	 *	* Check for use of unported `samplerExternalOES` 3.x type; not supported in 4.x, throw an error.
	 *	* Check for use of new keywords as identifiers; rename them if necessary.
	 */

	if (!preprocess_code()) {
		return false;
	}
	COND_MSG_FAIL(shader_mode == RS::SHADER_MAX, RTR("Shader type not a 3.x type."));
	err_str = "";
	curr_ptr = after_type_decl;
	auto check_deprecated_type = [&](TokE *type_pos) -> bool {
		if (type_pos->get().type == TT::TK_IDENTIFIER && has_removed_type(get_token_literal_text(type_pos->get()))) {
			const String err_msg = vformat(RTR("Deprecated type '%s' is not supported by this version of Godot."), get_token_literal_text(type_pos->get()));
			COND_LINE_MSG_FAIL(fail_on_unported, type_pos->get().line, err_msg);
			_add_comment_before(err_msg, type_pos);
		}
		return true;
	};

	// renaming changed hints
	Vector<TokE *> all_hints;
	for (auto &E : uniform_decls) {
		UniformDecl &uni = E.value;
		String name = get_token_literal_text(uni.name_pos->get());
		for (int i = 0; i < uni.hint_poses.size(); i++) {
			auto hint = uni.hint_poses[i];
			String hint_name = get_token_literal_text(hint->get());
			if (hint->get().type == TT::TK_IDENTIFIER && has_hint_replacement(hint_name)) {
				// replace the hint
				reset_to(hint);
				hint = replace_curr(mkTok(get_hint_replacement(hint_name)));
				uni.hint_poses.write[i] = hint;
				reset_to(after_type_decl);
			}
			all_hints.push_back(hint);
		}
	}

	// Renaming new reserved keywords used as identifiers (e.g "global", "instance").
	// To ensure idempotency, we only do this if we know for certain that the new keyword was used in a declaration.
	HashMap<TokenType, String> new_keyword_renames;
	HashSet<String> new_keyword_renames_values;
	HashMap<String, String> func_renames;
	auto insert_new_keyword_rename = [&](TokenType tk_type, const String &name) -> bool {
		if (tokentype_is_new_reserved_keyword(tk_type)) {
			if (!new_keyword_renames.has(tk_type)) {
				String rename = name + String("_");
				while (function_decls.has(rename) || uniform_decls.has(rename) || var_decls.has(rename)) {
					rename += "_";
				}
				new_keyword_renames[tk_type] = rename;
				new_keyword_renames_values.insert(rename);
			}
			return true;
		}
		return false;
	};
	for (auto &E : uniform_decls) {
		UniformDecl &uni = E.value;
		if (!check_deprecated_type(uni.type_pos)) {
			return false;
		}

		TokenType type = uni.name_pos->get().type;
		if (insert_new_keyword_rename(type, get_token_literal_text(uni.name_pos->get()))) {
			reset_to(uni.name_pos);
			uni.name_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, new_keyword_renames[type]));
			reset_to(after_type_decl);
		}
	}
	for (auto &E : var_decls) {
		if (E.value.is_empty()) {
			continue;
		}
		// Check for deprecated type.
		for (int i = 0; i < E.value.size(); i++) {
			if (!check_deprecated_type(E.value[i].type_pos)) {
				return false;
			}
		}

		auto &var = E.value[0];
		TT type = var.name_pos->get().type;
		if (insert_new_keyword_rename(type, get_token_literal_text(var.name_pos->get()))) {
			for (int i = 0; i < E.value.size(); i++) {
				// replace the identifier

				reset_to(E.value[i].name_pos);
				if (E.value[i].name_pos == E.value[i].start_pos) {
					E.value.write[i].name_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, new_keyword_renames[type]));
					E.value.write[i].start_pos = E.value.write[i].name_pos;
				} else {
					E.value.write[i].name_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, new_keyword_renames[type]));
				}
				reset_to(after_type_decl);
			}
		}
	}
	for (auto &E : function_decls) {
		auto &var = E.value;
		TT tok_type = var.name_pos->get().type;
		TT return_type = var.type_pos->get().type;
		if (!check_deprecated_type(var.type_pos)) {
			return false;
		}
		String name = get_token_literal_text(var.name_pos->get());
		if (is_renamed_function(shader_mode, name) && return_type == get_renamed_function_type(name)) {
			// replace the function name
			reset_to(var.name_pos);
			var.name_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, get_renamed_function(name)));
			reset_to(after_type_decl);
			String rename = get_renamed_function(name);
			func_renames[name] = rename;
			// Only doing this because "process" is a common word and we don't want to clobber an existing function named that.
			if (function_decls.has(rename)) {
				String rerename = rename + String("_");
				while (function_decls.has(rerename) || uniform_decls.has(rerename) || var_decls.has(rerename) || new_keyword_renames_values.has(rerename)) {
					rerename += "_";
				}
				func_renames[rename] = rerename;
				FunctionDecl &rere_func = function_decls[rename];
				reset_to(rere_func.name_pos);
				rere_func.name_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, rerename));
				reset_to(after_type_decl);
			}
		} else if (new_builtin_funcs.has(name)) {
			// We are not renaming the function if it matches a new built-in function name.
			// Unlikely to occur in legit 3.x scripts, legal 4.x code in certain instances, and way too much potential for mis-replacements.
			// Instead, we'll just add a comment.
			_add_comment_before(vformat(RTR("WARNING: Function '%s' is a builtin function in this version of Godot."), name), var.start_pos);
		} else if (insert_new_keyword_rename(tok_type, name)) {
			reset_to(var.name_pos);
			var.name_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, new_keyword_renames[tok_type]));
			reset_to(after_type_decl);
		}
	}
	bool in_function = false;
	String curr_func = "<global>";
	reset_to(after_type_decl);
	static Vector<String> uniform_qualifiers = { "global", "instance" };
	while (true) {
		TokE *cur_tok = get_next_token();
		if (cur_tok->get().type == TT::TK_EOF) {
			break;
		}
		for (auto &E : function_decls) {
			auto &func = E.value;
			if (cur_tok == func.body_start_pos) {
				in_function = true;
				curr_func = E.key; // The key is the ORIGINAL function name, not the potentially renamed one.
			} else if (in_function && cur_tok == func.body_end_pos) {
				in_function = false;
				curr_func = "<global>";
			}
		}
		if (cur_tok->get().type == TT::TK_STRUCT) {
			if (!_skip_struct()) {
				return false;
			}
			continue;
		}

		if (cur_tok->get().pos != NEW_IDENT && new_keyword_renames.has(cur_tok->get().type) && scope_has_decl(curr_func, get_token_literal_text(cur_tok->get()))) {
			String tok_text = get_token_literal_text(cur_tok->get());
			// Just extra insurance against replacing legit new keywords.
			if (uniform_qualifiers.has(tok_text)) {
				if (peek_next_tk_type() == TT::TK_UNIFORM) {
					continue; // Don't replace uniform qualifiers
				}
			} else if (all_hints.has(cur_tok)) {
				continue; // Hint, don't replace it.
			} else if (peek_prev_tk_type() == TT::TK_PERIOD) {
				continue; // Struct member access, don't replace it.
			}
			cur_tok = replace_curr(mkTok(TT::TK_IDENTIFIER, new_keyword_renames[cur_tok->get().type]));
			continue;
		}
		switch (cur_tok->get().type) {
			case TT::TK_FLOAT_CONSTANT: {
				// Earlier versions of Godot 3.x (< 3.5) allowed the use of the `f` sigil with float constants without a decimal place.
				String const_str = get_token_literal_text(cur_tok->get());
				if (const_str.ends_with("f") && !const_str.contains(".") && !const_str.contains("e")) {
					const_str = const_str.substr(0, const_str.length() - 1) + ".0f";
					cur_tok = replace_curr(mkTok(TT::TK_FLOAT_CONSTANT, const_str, 0xdeadbeef));
				}
			} break;
			case TT::TK_RENDER_MODE: {
				// we only care about the ones for spatial
				if (shader_mode == RenderingServer::ShaderMode::SHADER_SPATIAL) {
					while (true) {
						TokE *next_tk = get_next_token();
						if (next_tk->get().type == TT::TK_IDENTIFIER) {
							String id_text = get_token_literal_text(next_tk->get());
							if (has_removed_render_mode(shader_mode, id_text)) {
								if (!can_remove_render_mode(id_text)) {
									const String err_msg = vformat(RTR("Deprecated render mode '%s' is not supported by this version of Godot."), id_text);
									COND_LINE_MSG_FAIL(fail_on_unported, next_tk->get().line, err_msg);
									_add_comment_before(err_msg, next_tk);
								} else {
									if (peek_next_tk_type() == TT::TK_COMMA) {
										TokE *comma = get_next_token();
										reset_to(next_tk); // Reset to the identifier.
										EOF_FAIL(comma->next());
										next_tk = _remove_from_curr_to(comma->next()); // Inclusive of comma.
									} else if (peek_prev_tk_type() == TT::TK_COMMA && peek_next_tk_type() == TT::TK_SEMICOLON) {
										TokE *end = get_next_token();
										reset_to(next_tk); // Back to identifier.
										next_tk = get_prev_token(); // comma
										next_tk = _remove_from_curr_to(end); // Exclusive of semi-colon.
										break; // we're at the end of the render_mode declaration
									} else if (peek_prev_tk_type() == TT::TK_RENDER_MODE && peek_next_tk_type() == TT::TK_SEMICOLON) {
										// remove the whole line
										TokE *semi = get_next_token();
										COND_LINE_MSG_FAIL(!semi->next(), semi->get().line, "Unexpected EOF???"); // We should always have an EOF token at the end of the stream
										reset_to(next_tk); // Back to identifier.
										next_tk = get_prev_token(); // render_mode
										next_tk = _remove_from_curr_to(semi->next()); // Inclusive of semi-colon.
										break;
									} else {
										// we shouldn't be here
										LINE_MSG_FAIL(next_tk->get().line, RTR("Unexpected token after render mode declaration."));
									}
								}
							} else if (is_renamed_render_mode(shader_mode, id_text)) {
								next_tk = replace_curr(mkTok(TT::TK_IDENTIFIER, get_render_mode_rename(id_text)));
							}
						} else {
							COND_LINE_MSG_FAIL(next_tk->get().type != TT::TK_COMMA && next_tk->get().type != TT::TK_SEMICOLON, next_tk->get().line, RTR("Expected ',' or ';' after render mode declaration."));
						}
						if (next_tk->get().type == TT::TK_SEMICOLON) {
							break;
						}
					}
				}
			} break;
			case TT::TK_IDENTIFIER: {
				if (cur_tok->get().pos == NEW_IDENT) { // Skip already-replaced identifiers.
					break;
				}
				if (peek_prev_tk_type() == TT::TK_PERIOD) {
					break; // Struct member access, don't replace it.
				}
				String id_text = get_token_literal_text(cur_tok->get());
				if (func_renames.has(id_text) && peek_next_tk_type() == TT::TK_PARENTHESIS_OPEN) { // function call
					cur_tok = replace_curr(mkTok(TT::TK_IDENTIFIER, func_renames[id_text]));
				} else if (is_removed_builtin(shader_mode, id_text, curr_func) && !scope_has_decl(curr_func, id_text)) {
					if (get_removed_builtin_type(id_text) == TT::TK_ERROR) {
						String err_str = vformat(RTR("Deprecated built-in '%s' is not supported by this version of Godot"), id_text);
						COND_LINE_MSG_FAIL(fail_on_unported, cur_tok->get().line, err_str);
						_add_comment_before(err_str, cur_tok);
					}
					COND_LINE_MSG_FAIL(!_insert_uniform_declaration(id_text), cur_tok->get().line, RTR("Failed to insert uniform declaration"));
					UniformDecl &uniform_decl = uniform_decls[id_text];
					all_hints.append_array(uniform_decl.hint_poses);
				} else if (id_text == "INDEX" && has_builtin_rename(shader_mode, id_text, curr_func) && !scope_has_decl(curr_func, id_text)) {
					// INDEX was an int in 3.x, but is a uint in later versions.
					// Need to wrap it in a `int()` cast.
					// This is idempotent because this will only trigger if the `particles` function is "vertex" (which is renamed to "process").

					// Don't do this if it's singularly wrapped in a int(), uint() or float().
					if (peek_prev_tk_type() == TT::TK_PARENTHESIS_OPEN && peek_next_tk_type() == TT::TK_PARENTHESIS_CLOSE) {
						TT peeked_type = peek_prev_tk_type(2);
						if (peeked_type == TT::TK_TYPE_INT || peeked_type == TT::TK_TYPE_UINT || peeked_type == TT::TK_TYPE_FLOAT) {
							break;
						}
					}
					insert_before({ mkTok(TT::TK_TYPE_INT), mkTok(TT::TK_PARENTHESIS_OPEN) }, cur_tok);
					insert_after(mkTok(TT::TK_PARENTHESIS_CLOSE), cur_tok);
				} else if (id_text == "CLEARCOAT_GLOSS" && has_builtin_rename(shader_mode, id_text, curr_func) && !scope_has_decl(curr_func, id_text)) {
					cur_tok = replace_curr(mkTok(TT::TK_IDENTIFIER, "CLEARCOAT_ROUGHNESS"));
					List<Token>::Element *assign_closure_end = nullptr;
					switch (peek_next_tk_type()) {
						case TT::TK_OP_ASSIGN:
						case TT::TK_OP_ASSIGN_ADD:
						case TT::TK_OP_ASSIGN_SUB:
						case TT::TK_OP_ASSIGN_MUL:
						case TT::TK_OP_ASSIGN_DIV: {
							assign_closure_end = _get_end_of_closure();
							EOF_FAIL(assign_closure_end);

							TokE *assign_tk = get_next_token();
							TokE *insert_pos = assign_tk;
							if (assign_tk->next() && assign_tk->next()->get().type == TT::TK_SPACE) {
								insert_pos = assign_tk->next();
							}
							// " = (1.0 - ("
							Vector<Token> assign_prefix = {
								mkTok(TT::TK_OP_ASSIGN),
								mkTok(TT::TK_SPACE),
								mkTok(TT::TK_PARENTHESIS_OPEN),
								mkTok(TT::TK_FLOAT_CONSTANT, {}, 1.0),
								mkTok(TT::TK_SPACE),
								mkTok(TT::TK_OP_SUB),
								mkTok(TT::TK_SPACE),
								mkTok(TT::TK_PARENTHESIS_OPEN),
							};
							if (assign_tk->get().type != TT::TK_OP_ASSIGN) {
								// " = (1.0 - ((1.0 - CLEARCOAT_ROUGHNESS) {op}
								assign_prefix.append_array(
										{ mkTok(TT::TK_PARENTHESIS_OPEN),
												mkTok(TT::TK_FLOAT_CONSTANT, {}, 1.0),
												mkTok(TT::TK_SPACE),
												mkTok(TT::TK_OP_SUB),
												mkTok(TT::TK_SPACE),
												mkTok(TT::TK_IDENTIFIER, "CLEARCOAT_ROUGHNESS"),
												mkTok(TT::TK_PARENTHESIS_CLOSE),
												mkTok(TT::TK_SPACE) });
							}
							switch (assign_tk->get().type) {
								case TT::TK_OP_ASSIGN_ADD: {
									assign_prefix.append_array({ mkTok(TT::TK_OP_ADD), mkTok(TT::TK_SPACE) });
								} break;
								case TT::TK_OP_ASSIGN_SUB: {
									assign_prefix.append_array({ mkTok(TT::TK_OP_SUB), mkTok(TT::TK_SPACE) });
								} break;
								case TT::TK_OP_ASSIGN_MUL: {
									assign_prefix.append_array({ mkTok(TT::TK_OP_MUL), mkTok(TT::TK_SPACE) });
								} break;
								case TT::TK_OP_ASSIGN_DIV: {
									assign_prefix.append_array({ mkTok(TT::TK_OP_DIV), mkTok(TT::TK_SPACE) });
								} break;
								default:
									break;
							}
							insert_after(assign_prefix, insert_pos);

							// remove the assignment token
							if (assign_tk != insert_pos && insert_pos->next()) {
								// remove the extraneous space too if necessary
								_remove_from_curr_to(insert_pos->next()); // Exclusive of the token after the space
							} else {
								remove_cur_and_get_next();
							}
							// "))"
							insert_after({ mkTok(TT::TK_PARENTHESIS_CLOSE), mkTok(TT::TK_PARENTHESIS_CLOSE) }, assign_closure_end);
							reset_to(cur_tok);

						} break;

						default:
							break;
					}

					// Check for right-hand usage: if previous token is anything but a `{`, `}` or `;`.
					if (peek_prev_tk_type() == TT::TK_SEMICOLON ||
							peek_prev_tk_type() == TT::TK_CURLY_BRACKET_OPEN ||
							peek_prev_tk_type() == TT::TK_CURLY_BRACKET_CLOSE) {
						break;
					}

					// Invert right-hand usage
					Vector<Token> right_hand_prefix = { // "(1.0 - (";
						mkTok(TT::TK_PARENTHESIS_OPEN),
						mkTok(TT::TK_FLOAT_CONSTANT, {}, 1.0),
						mkTok(TT::TK_SPACE),
						mkTok(TT::TK_OP_SUB),
						mkTok(TT::TK_SPACE)
					};
					if (assign_closure_end) {
						right_hand_prefix.append_array({ mkTok(TT::TK_PARENTHESIS_OPEN) });
						insert_after({ mkTok(TT::TK_PARENTHESIS_CLOSE), mkTok(TT::TK_PARENTHESIS_CLOSE) }, assign_closure_end);
					} else {
						insert_after(mkTok(TT::TK_PARENTHESIS_CLOSE), cur_tok);
					}
					insert_before(right_hand_prefix, cur_tok);
				} else if (has_builtin_rename(shader_mode, id_text, curr_func) && !scope_has_decl(curr_func, id_text)) {
					cur_tok = replace_curr(mkTok(TT::TK_IDENTIFIER, get_builtin_rename(id_text)));
				}
			} break; // end of identifier case
			case TT::TK_ERROR: {
				LINE_MSG_FAIL(cur_tok->get().line, "Parser error ( " + cur_tok->get().text + ")");
			} break;
			default:
				break;
		}
	}
	return true;
}

String ShaderDeprecatedConverter::emit_code() const {
	if (code_tokens.size() == 0) {
		return "";
	}
	String new_code = "";
	const TokE *start = code_tokens.front()->next(); // skip TK_EOF token at start
	for (auto E = start; E; E = E->next()) {
		const Token &tk = E->get();
		ERR_FAIL_COND_V(tk.type < 0 || tk.type > TT::TK_MAX, "");
		bool end = false;
		switch (tk.type) {
			case TT::TK_ERROR:
			case TT::TK_EOF: {
				end = true;
			} break;
			default: {
				new_code += get_token_literal_text(tk);
			} break;
		}
		if (end) {
			break;
		}
	}

	return new_code;
}

void ShaderDeprecatedConverter::set_add_comments(bool p_add_comments) {
	add_comments = p_add_comments;
}
void ShaderDeprecatedConverter::set_fail_on_unported(bool p_fail_on_unported) {
	fail_on_unported = p_fail_on_unported;
}
void ShaderDeprecatedConverter::set_assume_correct(bool p_strict_preproc) {
	assume_correct = p_strict_preproc;
}

ShaderDeprecatedConverter::ShaderDeprecatedConverter(const String &p_code) :
		old_code(p_code) {
}

#endif // DISABLE_DEPRECATED
