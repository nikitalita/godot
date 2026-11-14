def can_build(env, platform):
    return True


def configure(env):
    pass

def get_opts(platform):
    from SCons.Variables import BoolVariable

    return [
        BoolVariable(
            "astcenc_export_templates",
            "Enable ASTC image compression in export template builds (increases binary size)",
            False,
        ),
    ]
