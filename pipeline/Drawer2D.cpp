#include <osg/Version>
#include <osg/io_utils>
#include <osg/ImageUtils>

#define BL_STATIC
#include <blend2d/blend2d.h>
#include "Drawer2D.h"

using namespace osgVerse;

struct BlendCore : public osg::Referenced
{
    BLImage image;
    BLContext* context;
    std::map<std::string, BLFontFace> fonts;
};

Drawer2D::Drawer2D() : _drawing(false)
{ _b2dData = new BlendCore; }

Drawer2D::Drawer2D(const Drawer2D& img, const osg::CopyOp& op)
    : osg::Image(img, op), _b2dData(img._b2dData), _drawing(img._drawing)
{}

Drawer2D::Drawer2D(const osg::Image& img, const osg::CopyOp& op)
    : osg::Image(img, op), _drawing(false) { _b2dData = new BlendCore; }

unsigned char* Drawer2D::convertImage(osg::Image* image, int& format, int& components)
{
    unsigned char* pixels = NULL; if (!image) return NULL;
    if (image->getDataType() != GL_UNSIGNED_BYTE)
    {
        OSG_WARN << "[Drawer2D] Invalid data type" << std::endl;
        return NULL;
    }

    switch (image->getPixelFormat())
    {
    case GL_LUMINANCE: case GL_ALPHA: case GL_RED:
        format = BLFormat::BL_FORMAT_A8; components = 1; pixels = image->data(); break;
    case GL_RGBA:
        format = BLFormat::BL_FORMAT_PRGB32; components = 4; pixels = image->data(); break;
    case GL_RGB:
        {
            osg::ref_ptr<osg::Image> dst = new osg::Image;
            dst->allocateImage(image->s(), image->t(), 1, GL_RGBA, GL_UNSIGNED_BYTE);
            osg::copyImage(image, 0, 0, 0, image->s(), image->t(), 1, dst.get(), 0, 0, 0);
            pixels = new unsigned char[dst->getTotalSizeInBytes()];
            memcpy(pixels, dst->data(), dst->getTotalSizeInBytes());
        }
        format = BLFormat::BL_FORMAT_XRGB32; components = 4; break;
    default:
        OSG_WARN << "[Drawer2D] Unsupported pixel format" << std::endl; return NULL;
    }
    return pixels;
}

bool Drawer2D::start(bool useCurrentPixels)
{
    int format = BLFormat::BL_FORMAT_PRGB32, numComponents = 0;
    int w = 1024; if (s() > 1) w = s();
    int h = 1024; if (t() > 1) h = t();

    unsigned char* pixels = NULL;
    if (useCurrentPixels) pixels = convertImage(this, format, numComponents);
    if (!_b2dData) _b2dData = new BlendCore;

    BlendCore* core = (BlendCore*)_b2dData.get();
    if (useCurrentPixels && pixels != NULL)
    {
        BLResult r = core->image.createFromData(
            w, h, (BLFormat)format, pixels, w * numComponents);
        if (r != BL_SUCCESS)
        {
            OSG_WARN << "[Drawer2D] Failed to use current data: " << r << std::endl;
            core->image.create(w, h, (BLFormat)format);
        }
    }
    else
        core->image.create(w, h, (BLFormat)format);

    BLContextCreateInfo info; info.threadCount = 0;
    core->context = new BLContext(core->image, &info);
    _drawing = true; return true;
}

bool Drawer2D::finish()
{
    BlendCore* core = (BlendCore*)_b2dData.get();
    bool contextClosed = false; _drawing = false;
    if (core && core->context != NULL)
    {
        BLImageData dataOut; core->context->end();
        if (core->image.getData(&dataOut) == BL_SUCCESS)
        {
            GLenum dType = GL_UNSIGNED_BYTE;
            bool omittedCopy = (data() == dataOut.pixelData);
            if (dataOut.size.w < 1 || dataOut.size.h < 1)
            {
                OSG_WARN << "[Drawer2D] Failed to get result data" << std::endl;
                return false;
            }
            else if (dataOut.format == BLFormat::BL_FORMAT_A8)
            {
                if (_pixelFormat != GL_RED && _pixelFormat != GL_ALPHA &&
                    _pixelFormat != GL_LUMINANCE)
                { allocateImage(dataOut.size.w, dataOut.size.h, 1, GL_LUMINANCE, dType); }
            }
            else if (dataOut.format == BLFormat::BL_FORMAT_PRGB32)
            {
                if (_pixelFormat != GL_RGBA)
                    allocateImage(dataOut.size.w, dataOut.size.h, 1, GL_RGBA, dType);
            }
            else if (dataOut.format == BLFormat::BL_FORMAT_XRGB32)
            {
                if (_pixelFormat == GL_RGB)
                {
                    osg::ref_ptr<osg::Image> src = new osg::Image; omittedCopy = true;
                    src->setImage(dataOut.size.w, dataOut.size.h, 1, GL_RGBA, GL_RGBA, dType,
                                  (unsigned char*)dataOut.pixelData, osg::Image::NO_DELETE);
                    osg::copyImage(src.get(), 0, 0, 0, src->s(), src->t(), 1, this, 0, 0, 0);
                }
                else
                    allocateImage(dataOut.size.w, dataOut.size.h, 1, GL_RGBA, dType);
            }

            if (!omittedCopy) memcpy(data(), dataOut.pixelData, getTotalSizeInBytes());
            dirty(); contextClosed = true;
        }
        else
            OSG_WARN << "[Drawer2D] Failed to get result data" << std::endl;
        delete core->context;
    }
    return contextClosed;
}

bool Drawer2D::loadFont(const std::string& name, const std::string& file)
{
    BlendCore* core = (BlendCore*)_b2dData.get();
    if (core != NULL)
    {
        BLFontFace& fontFace = core->fonts[name];
        BLArray<uint8_t> dataBuffer;
        if (BLFileSystem::readFile(file.c_str(), dataBuffer) == BL_SUCCESS)
        {
            BLFontData fontData;
            if (fontData.createFromData(dataBuffer) == BL_SUCCESS)
            {
                fontFace.reset();
                fontFace.createFromData(fontData, 0);
                return true;
            }
            else
                OSG_WARN << "[Drawer2D] Unable to create font: " << name << std::endl;
        }
        else OSG_WARN << "[Drawer2D] Unable to read font file: " << file << std::endl;
    }
    return false;
}

namespace osgVerse_Drawer
{
    template<typename T>
    void drawText(BLContext* context, const osg::Vec2f pos, BLFont& font,
                  const std::wstring& text, bool filled, const T& style)
    {
        if (filled)
            context->fillUtf16Text(
                BLPoint(pos[0], pos[1]), font, (uint16_t*)text.data(), text.length(), style);
        else
            context->strokeUtf16Text(
                BLPoint(pos[0], pos[1]), font, (uint16_t*)text.data(), text.length(), style);
    }

    template<typename T>
    void drawText(BLContext* context, const osg::Vec2f pos, BLFont& font,
                  const std::string& text, bool filled, const T& style)
    {
        if (filled)
            context->fillUtf8Text(
                BLPoint(pos[0], pos[1]), font, text.data(), text.length(), style);
        else
            context->strokeUtf8Text(
                BLPoint(pos[0], pos[1]), font, text.data(), text.length(), style);
    }

    template<typename T>
    void drawPolyline(BLContext* context, const std::vector<BLPoint>& blPts,
                      bool filled, bool closed, const T& style)
    {
        if (closed)
        {
            if (filled) context->fillPolygon(&blPts[0], blPts.size(), style);
            else context->strokePolygon(&blPts[0], blPts.size(), style);
        }
        else context->strokePolyline(&blPts[0], blPts.size(), style);
    }

    template<typename T>
    void drawCircle(BLContext* context, const osg::Vec2f pos0, float r1, float r2,
                    bool filled, const T& style)
    {
        if (filled)
        {
            if (r2 > 0.0f && !osg::equivalent(r2, r1))
                context->fillEllipse(pos0[0], pos0[1], r1, r2, style);
            else
                context->fillCircle(pos0[0], pos0[1], r1, style);
        }
        else
        {
            if (r2 > 0.0f && !osg::equivalent(r2, r1))
                context->strokeEllipse(pos0[0], pos0[1], r1, r2, style);
            else
                context->strokeCircle(pos0[0], pos0[1], r1, style);
        }
    }

    template<typename T>
    void drawArc(BLContext* context, const BLArc& arc, int chordOrPie,
                 bool filled, const T& style)
    {
        switch (chordOrPie)
        {
        case 1:  // Chord
            if (filled) context->fillChord(arc, style);
            else context->strokeChord(arc, style); break;
        case 2:  // Pie
            if (filled) context->fillPie(arc, style);
            else context->strokePie(arc, style); break;
        default:
            if (filled) context->fillChord(arc, style);
            else context->strokeArc(arc, style); break;
        }
    }

    template<typename T>
    void drawRectangle(BLContext* context, const osg::Vec4f r, float rx, float ry,
                       bool filled, const T& style)
    {
        BLRect rect{ r[0], r[1], r[2], r[3] };
        if (filled)
        {
            if (!(rx > 0.0f || ry > 0.0f)) context->fillRect(rect, style);
            else context->fillRoundRect(rect, rx, ry, style);
        }
        else
        {
            if (!(rx > 0.0f || ry > 0.0f)) context->strokeRect(rect, style);
            else context->strokeRoundRect(rect, rx, ry, style);
        }
    }

    template<typename T>
    void drawPath(BLContext* context, const BLPath& path, bool filled, const T& style)
    {
        if (filled) context->fillPath(path, style);
        else context->strokePath(path, style);
    }
}

#define VALID_B2D() BlendCore* core = (BlendCore*)_b2dData.get(); \
                    if (core && core->context && _drawing)
#define STYLE_CASES(func, ...) switch (sd.type) { \
        case StyleData::COLOR: func ( __VA_ARGS__, asColor(sd)); break; \
        case StyleData::IMAGE: func ( __VA_ARGS__, asPattern(sd)); break; \
        case StyleData::LINEAR_GRADIENT: func ( __VA_ARGS__, asLinearGradient(sd)); break; \
        default: OSG_WARN << "[Drawer2D] Unknown style: " << sd.type << std::endl; break; }

static BLRgba32 asColor(const Drawer2D::StyleData& sd)
{
    return BLRgba32(sd.color[0] * 255, sd.color[1] * 255,
                    sd.color[2] * 255, sd.color[3] * 255);
}

static BLPattern asPattern(const Drawer2D::StyleData& sd)
{
    BLImage texture; int format = BLFormat::BL_FORMAT_PRGB32, components = 0;
    unsigned char* pixels = Drawer2D::convertImage(sd.image.get(), format, components);
    texture.createFromData(sd.image->s(), sd.image->t(), (BLFormat)format,
                           pixels, sd.image->s() * components);
    return BLPattern(texture, BL_EXTEND_MODE_REPEAT);
}

static BLGradient asLinearGradient(const Drawer2D::StyleData& sd)
{
    BLGradient gradient(BLLinearGradientValues(
        sd.linearGradient[0], sd.linearGradient[1], sd.linearGradient[2], sd.linearGradient[3]));
    for (std::map<float, osg::Vec4f>::const_iterator itr = sd.gradientStops.begin();
         itr != sd.gradientStops.end(); ++itr)
    {
        const osg::Vec4f& c = itr->second;
        gradient.addStop(itr->first, BLRgba32(c[0] * 255, c[1] * 255, c[2] * 255, c[3] * 255));
    }
    return gradient;
}

void Drawer2D::drawText(const osg::Vec2f pos, float size, const std::wstring& text,
                        const std::string& fontName, const StyleData& sd)
{
    VALID_B2D()
    {
        if (core->fonts.empty())
        {
            OSG_WARN << "[Drawer2D] Unable to draw text without any font" << std::endl;
            return;
        }

        BLFontFace& fontFace = core->fonts.begin()->second;
        if (core->fonts.find(fontName) != core->fonts.end())
            fontFace = core->fonts[fontName];

        BLFont font; font.createFromFace(fontFace, size);
        STYLE_CASES(osgVerse_Drawer::drawText, core->context, pos, font, text, sd.filled);
    }
}

void Drawer2D::drawUtf8Text(const osg::Vec2f pos, float size, const std::string& text,
                            const std::string& fontName, const StyleData& sd)
{
    VALID_B2D()
    {
        if (core->fonts.empty())
        {
            OSG_WARN << "[Drawer2D] Unable to draw text without any font" << std::endl;
            return;
        }

        BLFontFace& fontFace = core->fonts.begin()->second;
        if (core->fonts.find(fontName) != core->fonts.end())
            fontFace = core->fonts[fontName];

        BLFont font; font.createFromFace(fontFace, size);
        STYLE_CASES(osgVerse_Drawer::drawText, core->context, pos, font, text, sd.filled);
    }
}

void Drawer2D::drawLine(const osg::Vec2f p0, const osg::Vec2f p1, const StyleData& sd)
{
    VALID_B2D()
    {
        STYLE_CASES(core->context->strokeLine, BLPoint(p0[0], p0[1]), BLPoint(p1[0], p1[1]));
    }
}

void Drawer2D::drawPolyline(const std::vector<osg::Vec2f>& points,
                            bool closed, const StyleData& sd)
{
    VALID_B2D()
    {
        if (points.size() < 2)
        {
            OSG_WARN << "[Drawer2D] Too few points for drawing polyline" << std::endl;
            return;
        }
        else if (points.size() == 2) closed = false;

        std::vector<BLPoint> blPts;
        for (size_t i = 0; i < points.size(); ++i)
            blPts.push_back(BLPoint(points[i].x(), points[i].y()));
        STYLE_CASES(osgVerse_Drawer::drawPolyline, core->context, blPts, sd.filled, closed);
    }
}

void Drawer2D::drawCircle(const osg::Vec2f pos0, float r1, float r2, const StyleData& sd)
{
    VALID_B2D()
    {
        STYLE_CASES(osgVerse_Drawer::drawCircle, core->context, pos0, r1, r2, sd.filled);
    }
}

void Drawer2D::drawArc(const osg::Vec2f pos0, float r1, float r2, float start, float sweep,
                       int asChordOrPie, const StyleData& sd)
{
    VALID_B2D()
    {
        BLArc arc(pos0[0], pos0[1], r1, r2, start, sweep);
        STYLE_CASES(osgVerse_Drawer::drawArc, core->context, arc, asChordOrPie, sd.filled);
    }
}

void Drawer2D::drawRectangle(const osg::Vec4f r, float rx, float ry, const StyleData& sd)
{
    VALID_B2D()
    {
        STYLE_CASES(osgVerse_Drawer::drawRectangle, core->context, r, rx, ry, sd.filled);
    }
}

void Drawer2D::drawPath(const std::vector<PathData>& path, const StyleData& sd)
{
    VALID_B2D()
    {
        if (path.size() < 2)
        {
            OSG_WARN << "[Drawer2D] Too few points for drawing path" << std::endl;
            return;
        }

        BLPath blPath;
        for (size_t i = 0; i < path.size(); ++i)
        {
            const PathData& pd = path[i];
            if (pd.isCubic)
            {
                blPath.cubicTo(BLPoint(pd.pos[0], pd.pos[1]),
                               BLPoint(pd.control0[0], pd.control0[1]),
                               BLPoint(pd.control1[0], pd.control1[1]));
            }
            else if (pd.isMoving) blPath.moveTo(BLPoint(pd.pos[0], pd.pos[1]));
            else blPath.lineTo(BLPoint(pd.pos[0], pd.pos[1]));
        }
        STYLE_CASES(osgVerse_Drawer::drawPath, core->context, blPath, sd.filled);
    }
}

void Drawer2D::setStrokeOption(StrokeOption opt, int v)
{
    VALID_B2D()
    {
        switch (opt)
        {
        case WIDTH:
            core->context->setStrokeWidth((double)v); break;
        case START_CAP:
            core->context->setStrokeCap(BL_STROKE_CAP_POSITION_START, (BLStrokeCap)v); break;
        case END_CAP:
            core->context->setStrokeCap(BL_STROKE_CAP_POSITION_END, (BLStrokeCap)v); break;
        case JOIN:
            core->context->setStrokeJoin((BLStrokeJoin)v); break;
        case DASH_OFFSET:
            core->context->setStrokeDashOffset((double)v); break;
        default:
            OSG_WARN << "[Drawer2D] Unknown stroke option: " << opt << std::endl; break;
        }
    }
}

void Drawer2D::translate(const osg::Vec2& pos)
{
    VALID_B2D() { core->context->translate(pos[0], pos[1]); }
}

void Drawer2D::scale(const osg::Vec2& scale)
{
    VALID_B2D() { core->context->scale(scale[0], scale[1]); }
}

void Drawer2D::rotate(float angle)
{
    VALID_B2D() { core->context->rotate(angle); }
}

void Drawer2D::clear(const osg::Vec4f& r)
{
    VALID_B2D()
    {
        if (r[2] > 0.0f && r[3] > 0.0f)
            core->context->clearRect(BLRect(r[0], r[1], r[2], r[3]));
        else
            core->context->clearAll();
    }
}

void Drawer2D::fillBackground(const osg::Vec4f& c)
{
    VALID_B2D()
    {
        BLRgba32 bg(c[0] * 255, c[1] * 255, c[2] * 255, c[3] * 255);
        core->context->fillAll(bg);
    }
}
