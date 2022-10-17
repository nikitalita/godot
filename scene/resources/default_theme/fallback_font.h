#ifndef __FALLBACK_FONT_H__
#define __FALLBACK_FONT_H__

#include "scene/resources/font.h"
class FallbackFonts {
private:
	static FallbackFonts *singleton;

	HashMap<String, TypedArray<Font>> _fallback_font_map;
#ifdef TOOLS_ENABLED
	HashMap<String, TypedArray<Font>> _editor_fallback_font_map;
#endif

	void _load_internal_default_fallback_fonts(TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf = false, bool editor = false);
	void _load_system_default_fallback_fonts(TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf = false, bool editor = false);
	void _load_default_fallback_fonts(TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf = false, bool editor = false);

public:
	static FallbackFonts *get_singleton();

	static Ref<FontFile> load_external_font(const String &p_path, TextServer::Hinting p_hinting, TextServer::FontAntialiasing p_aa, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel_positioning, bool p_msdf = false, TypedArray<Font> *r_fallbacks = nullptr);
	static Ref<SystemFont> load_system_font(const String &p_name, bool p_bold, TextServer::Hinting p_hinting, TextServer::FontAntialiasing p_aa, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel_positioning, bool p_msdf = false, TypedArray<Font> *r_fallbacks = nullptr);
	static Ref<FontFile> load_internal_font(const uint8_t *p_data, size_t p_size, TextServer::Hinting p_hinting, TextServer::FontAntialiasing p_aa, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel_positioning, bool p_msdf = false, TypedArray<Font> *r_fallbacks = nullptr);
	static Ref<FontVariation> make_bold_font(const Ref<Font> &p_font, double p_embolden, TypedArray<Font> *r_fallbacks);
	static Ref<FontVariation> make_bold_italic_font(const Ref<Font> &p_font, double p_embolden, TypedArray<Font> *r_fallbacks);
	static Ref<FontVariation> make_italic_font(const Ref<Font> &p_font, TypedArray<Font> *r_fallbacks = nullptr);
	HashMap<String, TypedArray<Font>> get_fallback_fonts(bool bold, bool italic, TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf, bool p_editor);
#ifdef TOOLS_ENABLED
	void load_default_editor_fallback_fonts(TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf = false);
	void set_editor_fallback_fonts(Ref<Font> &p_font, bool bold, bool italic);
	bool are_default_editor_fallback_fonts_loaded();

#endif
	bool are_default_fallback_fonts_loaded();
	void load_default_fallback_fonts(TextServer::Hinting p_font_hinting, TextServer::FontAntialiasing p_font_antialiasing, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel, bool p_msdf = false);
	void set_fallback_fonts(Ref<Font> &p_font, bool bold, bool italic);
	FallbackFonts();
	~FallbackFonts();
};
#endif // __FALLBACK_FONT_H__
