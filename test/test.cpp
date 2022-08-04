#include "hlslcc.h"

#include <fstream>
#include <string>
#include <iostream>

int main()
{
    unsigned int flags = HLSLCC_FLAG_UNIFORM_BUFFER_OBJECT;

    GlExtensions ext;
    ext.ARB_explicit_attrib_location = 1;
    ext.ARB_explicit_uniform_location = 1;
    ext.ARB_shading_language_420pack = 0;
    ext.OVR_multiview = 0;
    ext.EXT_shader_framebuffer_fetch = 0;

    HLSLccSamplerPrecisionInfo info;
    HLSLccReflection reflection;
    GLSLShader shader;
    int ok = TranslateHLSLFromFile("test/a.dxbc", 0, LANG_DEFAULT, &ext, nullptr, info, reflection, &shader);

    if (ok)
    {
        std::ofstream out("test/out.hlsl");
        out << shader.sourceCode;
        out.close();
    }
    else
    {
        printf("HLSLcc failed with error code %i\n", ok);
    }

    return 0;
}
