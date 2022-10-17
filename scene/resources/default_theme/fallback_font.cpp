#include "fallback_font.h"
#ifdef TOOLS_ENABLED
#include "editor/builtin_fonts.gen.h"
#else
#include "default_font.gen.h"
#endif

FallbackFonts *FallbackFonts::singleton = nullptr;
FallbackFonts *FallbackFonts::get_singleton() {
	return singleton;
}

Ref<FontFile> FallbackFonts::load_external_font(const String &p_path, TextServer::Hinting p_hinting, TextServer::FontAntialiasing p_aa, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel_positioning, bool p_msdf, TypedArray<Font> *r_fallbacks) {
	Ref<FontFile> font;
	font.instantiate();

	Vector<uint8_t> data = FileAccess::get_file_as_array(p_path);

	font->set_data(data);
	font->set_multichannel_signed_distance_field(p_msdf);
	font->set_antialiasing(p_aa);
	font->set_hinting(p_hinting);
	font->set_force_autohinter(p_autohint);
	font->set_subpixel_positioning(p_font_subpixel_positioning);

	if (r_fallbacks != nullptr) {
		r_fallbacks->push_back(font);
	}

	return font;
}

Ref<SystemFont> FallbackFonts::load_system_font(const String &p_name, bool p_bold, bool p_italic, TextServer::Hinting p_hinting, TextServer::FontAntialiasing p_aa, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel_positioning, bool p_msdf, TypedArray<Font> *r_fallbacks) {
	Ref<SystemFont> font;
	font.instantiate();

	PackedStringArray names;
	names.push_back(p_name);

	font->set_font_names(names);
	if (p_bold) {
		font->set_font_style(TextServer::FONT_BOLD);
	}
	if (p_italic) {
		font->set_font_style(TextServer::FONT_ITALIC);
	}
	font->set_multichannel_signed_distance_field(p_msdf);
	font->set_antialiasing(p_aa);
	font->set_hinting(p_hinting);
	font->set_force_autohinter(p_autohint);
	font->set_subpixel_positioning(p_font_subpixel_positioning);

	if (r_fallbacks != nullptr) {
		r_fallbacks->push_back(font);
	}

	return font;
}

Ref<FontFile> FallbackFonts::load_internal_font(const uint8_t *p_data, size_t p_size, TextServer::Hinting p_hinting, TextServer::FontAntialiasing p_aa, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel_positioning, bool p_msdf, TypedArray<Font> *r_fallbacks) {
	Ref<FontFile> font;
	font.instantiate();

	font->set_data_ptr(p_data, p_size);
	font->set_multichannel_signed_distance_field(p_msdf);
	font->set_antialiasing(p_aa);
	font->set_hinting(p_hinting);
	font->set_force_autohinter(p_autohint);
	font->set_subpixel_positioning(p_font_subpixel_positioning);

	if (r_fallbacks != nullptr) {
		r_fallbacks->push_back(font);
	}

	return font;
}

Ref<FontVariation> FallbackFonts::make_bold_font(const Ref<Font> &p_font, double p_embolden, TypedArray<Font> *r_fallbacks) {
	Ref<FontVariation> font_var;
	font_var.instantiate();
	font_var->set_base_font(p_font);
	font_var->set_variation_embolden(p_embolden);

	if (r_fallbacks != nullptr) {
		r_fallbacks->push_back(font_var);
	}

	return font_var;
}
Ref<FontVariation> FallbackFonts::make_bold_italic_font(const Ref<Font> &p_font, double p_embolden, TypedArray<Font> *r_fallbacks) {
	Ref<FontVariation> font_var;
	font_var.instantiate();
	font_var->set_base_font(p_font);
	font_var->set_variation_embolden(p_embolden);
	font_var->set_variation_transform(Transform2D(1.0, 0.2, 0.0, 1.0, 0.0, 0.0));

	if (r_fallbacks != nullptr) {
		r_fallbacks->push_back(font_var);
	}

	return font_var;
}

Ref<FontVariation> FallbackFonts::make_italic_font(const Ref<Font> &p_font, TypedArray<Font> *r_fallbacks) {
	Ref<FontVariation> font_var;
	font_var.instantiate();
	font_var->set_base_font(p_font);
	font_var->set_variation_transform(Transform2D(1.0, 0.2, 0.0, 1.0, 0.0, 0.0));

	if (r_fallbacks != nullptr) {
		r_fallbacks->push_back(font_var);
	}

	return font_var;
}

void FallbackFonts::_load_internal_default_fallback_fonts(TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf, bool editor) {
	const float embolden_strength = 0.6;
#if defined(FALLBACK_FONTS_ENABLED) || defined(TOOLS_ENABLED)
	TypedArray<Font> fallbacks;
	Ref<FontFile> arabic_font = load_internal_font(_font_NotoNaskhArabicUI_Regular, _font_NotoNaskhArabicUI_Regular_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> bengali_font = load_internal_font(_font_NotoSansBengaliUI_Regular, _font_NotoSansBengaliUI_Regular_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> devanagari_font = load_internal_font(_font_NotoSansDevanagariUI_Regular, _font_NotoSansDevanagariUI_Regular_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> georgian_font = load_internal_font(_font_NotoSansGeorgian_Regular, _font_NotoSansGeorgian_Regular_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> hebrew_font = load_internal_font(_font_NotoSansHebrew_Regular, _font_NotoSansHebrew_Regular_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> malayalam_font = load_internal_font(_font_NotoSansMalayalamUI_Regular, _font_NotoSansMalayalamUI_Regular_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> oriya_font = load_internal_font(_font_NotoSansOriyaUI_Regular, _font_NotoSansOriyaUI_Regular_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> sinhala_font = load_internal_font(_font_NotoSansSinhalaUI_Regular, _font_NotoSansSinhalaUI_Regular_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> tamil_font = load_internal_font(_font_NotoSansTamilUI_Regular, _font_NotoSansTamilUI_Regular_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> telugu_font = load_internal_font(_font_NotoSansTeluguUI_Regular, _font_NotoSansTeluguUI_Regular_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> thai_font = load_internal_font(_font_NotoSansThaiUI_Regular, _font_NotoSansThaiUI_Regular_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> fallback_font = load_internal_font(_font_DroidSansFallback, _font_DroidSansFallback_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	Ref<FontFile> japanese_font = load_internal_font(_font_DroidSansJapanese, _font_DroidSansJapanese_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks);
	if (editor) {
#if defined(TOOLS_ENABLED)
		_editor_fallback_font_map.insert("regular", fallbacks);
#endif
	} else {
		_fallback_font_map.insert("regular", fallbacks);
	}
	TypedArray<Font> fallbacks_bold;
	Ref<FontFile> arabic_font_bold = load_internal_font(_font_NotoNaskhArabicUI_Bold, _font_NotoNaskhArabicUI_Bold_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks_bold);
	Ref<FontFile> bengali_font_bold = load_internal_font(_font_NotoSansBengaliUI_Bold, _font_NotoSansBengaliUI_Bold_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks_bold);
	Ref<FontFile> devanagari_font_bold = load_internal_font(_font_NotoSansDevanagariUI_Bold, _font_NotoSansDevanagariUI_Bold_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks_bold);
	Ref<FontFile> georgian_font_bold = load_internal_font(_font_NotoSansGeorgian_Bold, _font_NotoSansGeorgian_Bold_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks_bold);
	Ref<FontFile> hebrew_font_bold = load_internal_font(_font_NotoSansHebrew_Bold, _font_NotoSansHebrew_Bold_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks_bold);
	Ref<FontFile> malayalam_font_bold = load_internal_font(_font_NotoSansMalayalamUI_Bold, _font_NotoSansMalayalamUI_Bold_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks_bold);
	Ref<FontFile> oriya_font_bold = load_internal_font(_font_NotoSansOriyaUI_Bold, _font_NotoSansOriyaUI_Bold_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks_bold);
	Ref<FontFile> sinhala_font_bold = load_internal_font(_font_NotoSansSinhalaUI_Bold, _font_NotoSansSinhalaUI_Bold_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks_bold);
	Ref<FontFile> tamil_font_bold = load_internal_font(_font_NotoSansTamilUI_Bold, _font_NotoSansTamilUI_Bold_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks_bold);
	Ref<FontFile> telugu_font_bold = load_internal_font(_font_NotoSansTeluguUI_Bold, _font_NotoSansTeluguUI_Bold_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks_bold);
	Ref<FontFile> thai_font_bold = load_internal_font(_font_NotoSansThaiUI_Bold, _font_NotoSansThaiUI_Bold_size, p_font_hinting, p_font_antialiasing, true, p_font_subpixel, false, &fallbacks_bold);
	Ref<FontVariation> fallback_font_bold = make_bold_font(fallback_font, embolden_strength, &fallbacks_bold);
	Ref<FontVariation> japanese_font_bold = make_bold_font(japanese_font, embolden_strength, &fallbacks_bold);
	if (editor) {
#if defined(TOOLS_ENABLED)
		_editor_fallback_font_map.insert("bold", fallbacks_bold);
#endif
		//We don't load italic versions for the editor
		return;
	} else {
		_fallback_font_map.insert("bold", fallbacks_bold);
	}
	TypedArray<Font> fallbacks_italic;
	Ref<FontFile> arabic_font_italic = make_italic_font(arabic_font, &fallbacks_italic);
	Ref<FontFile> bengali_font_italic = make_italic_font(bengali_font, &fallbacks_italic);
	Ref<FontFile> devanagari_font_italic = make_italic_font(devanagari_font, &fallbacks_italic);
	Ref<FontFile> georgian_font_italic = make_italic_font(georgian_font, &fallbacks_italic);
	Ref<FontFile> hebrew_font_italic = make_italic_font(hebrew_font, &fallbacks_italic);
	Ref<FontFile> malayalam_font_italic = make_italic_font(malayalam_font, &fallbacks_italic);
	Ref<FontFile> oriya_font_italic = make_italic_font(oriya_font, &fallbacks_italic);
	Ref<FontFile> sinhala_font_italic = make_italic_font(sinhala_font, &fallbacks_italic);
	Ref<FontFile> tamil_font_italic = make_italic_font(tamil_font, &fallbacks_italic);
	Ref<FontFile> telugu_font_italic = make_italic_font(telugu_font, &fallbacks_italic);
	Ref<FontFile> thai_font_italic = make_italic_font(thai_font, &fallbacks_italic);
	Ref<FontVariation> fallback_font_italic = make_italic_font(fallback_font, &fallbacks_italic);
	Ref<FontVariation> japanese_font_italic = make_italic_font(japanese_font, &fallbacks_italic);
	_fallback_font_map.insert("italic", fallbacks_italic);
	TypedArray<Font> fallbacks_bold_italic;
	Ref<FontFile> arabic_font_bolditalic = make_italic_font(arabic_font_bold, &fallbacks_bold_italic);
	Ref<FontFile> bengali_font_bolditalic = make_italic_font(bengali_font_bold, &fallbacks_bold_italic);
	Ref<FontFile> devanagari_font_bolditalic = make_italic_font(devanagari_font_bold, &fallbacks_bold_italic);
	Ref<FontFile> georgian_font_bolditalic = make_italic_font(georgian_font_bold, &fallbacks_bold_italic);
	Ref<FontFile> hebrew_font_bolditalic = make_italic_font(hebrew_font_bold, &fallbacks_bold_italic);
	Ref<FontFile> malayalam_font_bolditalic = make_italic_font(malayalam_font_bold, &fallbacks_bold_italic);
	Ref<FontFile> oriya_font_bolditalic = make_italic_font(oriya_font_bold, &fallbacks_bold_italic);
	Ref<FontFile> sinhala_font_bolditalic = make_italic_font(sinhala_font_bold, &fallbacks_bold_italic);
	Ref<FontFile> tamil_font_bolditalic = make_italic_font(tamil_font_bold, &fallbacks_bold_italic);
	Ref<FontFile> telugu_font_bolditalic = make_italic_font(telugu_font_bold, &fallbacks_bold_italic);
	Ref<FontFile> thai_font_bolditalic = make_italic_font(thai_font_bold, &fallbacks_bold_italic);
	Ref<FontVariation> fallback_font_bolditalic = make_italic_font(fallback_font_bold, &fallbacks_bold_italic);
	Ref<FontVariation> japanese_font_bolditalic = make_italic_font(japanese_font_bold, &fallbacks_bold_italic);
	_fallback_font_map.insert("bold_italic", fallbacks_bold_italic);
#endif
}

HashMap<String, String> FallbackFonts::_probe_system_fonts(const Vector<String> &needed_scripts, bool bold, bool italic) {
	Vector<String> sys_font_names = OS::get_singleton()->get_system_fonts();
	HashMap<String, String> selected_scripts;
	for (const String &E : sys_font_names) {
		String path = OS::get_singleton()->get_system_font_path(E, bold, italic);
		if (path.is_empty()) {
			continue;
		}

		Ref<FontFile> f;
		f.instantiate();
		if (f->load_dynamic_font(path) == OK) {
			for (String needed_script : needed_scripts) {
				if (!selected_scripts.has(needed_script) && f->is_script_supported(needed_script)) {
					bool ok = true;
					String sample = _samples[needed_script];
					for (int j = 0; j < sample.size(); j++) {
						bool has_char = f->has_char(sample[j]);
						ok = ok && has_char;
						if (!has_char) {
							break;
						}
					}
					if (ok) {
						selected_scripts.insert(needed_script, E);
						print_line(vformat("Selected font for %s: %s (%s)", needed_script, E, path));
						break;
					}
				}
			}
		}
	}
	return selected_scripts;
}

void FallbackFonts::_load_system_default_fallback_fonts(TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf, bool editor) {
	TypedArray<Font> fb_fonts;
	TypedArray<Font> fb_fonts_bold;
	TypedArray<Font> fb_fonts_italic;
	TypedArray<Font> fb_fonts_bold_italic;
	HashMap<String, String> selected_script_presets;
	Vector<String> needed_scripts;
	int script_set_size = 0;
	for (auto E : _samples) {
		Vector<String> script_fallbacks = OS::get_singleton()->get_system_font_preset_fallbacks_names("sans-serif", E.key);
		if (script_fallbacks.is_empty()) {
			needed_scripts.push_back(E.key);
			continue;
		}
		for (String fallback : script_fallbacks) {
			String path = OS::get_singleton()->get_system_font_path(fallback);
			if (!path.is_empty()) {
				selected_script_presets.insert(E.key, fallback);
				break;
			}
		}
		if (!selected_script_presets.has(E.key)) {
			needed_scripts.push_back(E.key);
		}
	}
	// If we didn't find presets for some scripts, as a last resort, we probe the fonts to see if it has fonts
	// that support the needed scripts
	HashMap<String, String> probed_fonts = _probe_system_fonts(needed_scripts, false, false);
	Vector<String> fb_font_names;
	for (auto probed_font : probed_fonts) {
		selected_script_presets.insert(probed_font.key, probed_font.value);
	}

	for (auto E : selected_script_presets) {
		String font_name = E.value;
		load_system_font(font_name, false, false, p_font_hinting, p_font_antialiasing, p_autohint, p_font_subpixel, p_msdf, &fb_fonts);
		load_system_font(font_name, true, false, p_font_hinting, p_font_antialiasing, p_autohint, p_font_subpixel, p_msdf, &fb_fonts_bold);
		if (!editor) {
			load_system_font(font_name, false, true, p_font_hinting, p_font_antialiasing, p_autohint, p_font_subpixel, p_msdf, &fb_fonts_italic);
			load_system_font(font_name, true, true, p_font_hinting, p_font_antialiasing, p_autohint, p_font_subpixel, p_msdf, &fb_fonts_bold_italic);
		}
	}
	if (editor) {
#if defined(TOOLS_ENABLED)
		_editor_fallback_font_map.insert("regular", fb_fonts);
		_editor_fallback_font_map.insert("bold", fb_fonts_bold);
#endif
	} else {
		_fallback_font_map.insert("regular", fb_fonts);
		_fallback_font_map.insert("bold", fb_fonts_bold);
		_fallback_font_map.insert("italic", fb_fonts_italic);
		_fallback_font_map.insert("bold_italic", fb_fonts_bold_italic);
	}
}

void FallbackFonts::_load_default_fallback_fonts(TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf, bool editor) {
#if defined(TOOLS_ENABLED)
	if (editor) {
		_load_internal_default_fallback_fonts(p_font_hinting, p_font_antialiasing, p_autohint, p_font_subpixel, p_msdf, editor);
		return;
	}
#endif
#if defined(FALLBACK_FONTS_ENABLED)
	_load_internal_default_fallback_fonts(p_font_hinting, p_font_antialiasing, p_autohint, p_font_subpixel, p_msdf, false);
#else
	_load_system_default_fallback_fonts(p_font_hinting, p_font_antialiasing, p_autohint, p_font_subpixel, p_msdf, false);
#endif
}

bool FallbackFonts::are_default_fallback_fonts_loaded() {
	return _fallback_font_map.size() > 0;
}
void FallbackFonts::load_default_fallback_fonts(TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf) {
	_load_default_fallback_fonts(p_font_hinting, p_font_antialiasing, p_autohint, p_font_subpixel, p_msdf, false);
}
#if defined(TOOLS_ENABLED)
void FallbackFonts::load_default_editor_fallback_fonts(TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf) {
	_load_default_fallback_fonts(p_font_hinting, p_font_antialiasing, p_autohint, p_font_subpixel, p_msdf, true);
}
bool FallbackFonts::are_default_editor_fallback_fonts_loaded() {
	return _editor_fallback_font_map.size() > 0;
}

void FallbackFonts::set_editor_fallback_fonts(Ref<Font> &p_font, bool bold, bool italic) {
	if (bold && italic) {
		p_font->set_fallbacks(FallbackFonts::_editor_fallback_font_map["bold_italic"]);
	} else if (bold) {
		p_font->set_fallbacks(FallbackFonts::_editor_fallback_font_map["bold"]);
	} else if (italic) {
		p_font->set_fallbacks(FallbackFonts::_editor_fallback_font_map["italic"]);
	} else {
		p_font->set_fallbacks(FallbackFonts::_editor_fallback_font_map["regular"]);
	}
}
#endif

void FallbackFonts::set_fallback_fonts(Ref<Font> &p_font, bool bold, bool italic) {
	if (bold && italic) {
		p_font->set_fallbacks(FallbackFonts::_fallback_font_map["bold_italic"]);
	} else if (bold) {
		p_font->set_fallbacks(FallbackFonts::_fallback_font_map["bold"]);
	} else if (italic) {
		p_font->set_fallbacks(FallbackFonts::_fallback_font_map["italic"]);
	} else {
		p_font->set_fallbacks(FallbackFonts::_fallback_font_map["regular"]);
	}
}

HashMap<String, TypedArray<Font>> FallbackFonts::get_fallback_fonts(bool bold, bool italic, TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf, bool p_editor) {
#ifdef TOOLS_ENABLED
	if (p_editor) {
	}
#endif
#ifdef FALLBACK_FONTS_ENABLED
	//_load_internal_default_fallback_fonts(bold, italic, p_font_hinting, p_font_antialiasing, p_autohint, p_font_subpixel, p_msdf);
#endif
	return HashMap<String, TypedArray<Font>>();
	// TODO: get system fonts
}

FallbackFonts::FallbackFonts() {
	singleton = this;
	_samples.insert("Arab", U"يوٱهنملكقفغعظطضصشسزرذدخحجثتبا");
	_samples.insert("Beng", U"অআইঈউঊঋৠঌৡএঐওঔকখগঘঙচছজঝঞটঠডঢণতথদধনপফবভমযয়রলওয়শষসহক্ষজ্ঞৎ");
	_samples.insert("Deva", U"अआइईउऊऋॠऌॡएऐओऔकखगघङचछजझञटठडढणतथदधनपफबभमयरलळवशषसहक्षज्ञ");
	_samples.insert("Geor", U"აბგდევზთიკლმნოპჟრსტუფქღყშჩცძწჭხჯჰ");
	_samples.insert("Hebr", U"בגדהוזחטיכךלמנסעפצקרשתםןףץ");
	_samples.insert("Mlym", U"അആഇഈഉഊഋഌഎഏഐഒഓഔകഖഗഘങചഛജഝഞടഠഡഢണതഥദധനഩ");
	_samples.insert("Orya", U"ଅଆଇଈଉଊଋୠଌୡଏଐଓଔକଖଗଘଙଚଛଜଝଞଟଠଡଢଣତଥଦଧନପଫବଭମଯୟରଲଳୱଶଷସହକ୍ଷଜ୍ଞ");
	_samples.insert("Sinh", U"අආඇඈඉඊඋඌඍඎඏඐඑඒඓඔඕඖකඛගඝඞඟචඡජඣඤඥඦටඨඩඪණඬතථද");
	_samples.insert("Taml", U"ஆஇஈஉஊஎஏஐஒஓஔகஙசஜஞடணதநனபமயரறலளழவஶஷஸஹாிீுூெேை");
	_samples.insert("Telu", U"అఆఇఈఉఊఋఌఎఏఐఒఓఔకఖగఘఙచఛజఝఞటఠడఢణతథదధనపఫబభమయ");
	_samples.insert("Thai", U"กขฃคฅฆงจฉชซฌญฎฏฐฑฒณดตถทธนบปผฝพฟภมยรฤลฦวศษสหฬ");
	_samples.insert("Hani", U"一人大中的上出生不年自子地日本同下三小前所是我有了在国到会你他要以時也就可之得十事好那能学家多二和後用天者而心行新看文如道去都想方只手成問然当作主學这資長會来五這個个社市说们月为四為九交來政系業分时");
}

FallbackFonts::~FallbackFonts() {
	_samples.clear();
	_fallback_font_map.clear();
#ifdef TOOLS_ENABLED
	_editor_fallback_font_map.clear();
#endif
	singleton = nullptr;
}
