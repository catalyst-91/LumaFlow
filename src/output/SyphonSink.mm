// Syphon (macOS) stream output using SyphonMetalServer. Built only when the
// Syphon framework is found (LF_HAVE_SYPHON). Syphon is BSD licensed.
#include "StreamSinks.h"
#import <Metal/Metal.h>
#import <Syphon/Syphon.h>

namespace lf {

class SyphonSink : public VideoSink {
public:
    ~SyphonSink() override { close(); }
    bool open(const StreamConfig& cfg, int w, int h, double, QString* err) override {
        @autoreleasepool {
            m_device = MTLCreateSystemDefaultDevice();
            if (!m_device) { if (err) *err = "no Metal device"; return false; }
            m_queue = [m_device newCommandQueue];
            NSString* name = [NSString stringWithUTF8String:cfg.name.toUtf8().constData()];
            m_server = [[SyphonMetalServer alloc] initWithName:name device:m_device options:nil];
            if (!m_server) { if (err) *err = "SyphonMetalServer creation failed"; close(); return false; }
            MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:NSUInteger(w) height:NSUInteger(h) mipmapped:NO];
            d.usage = MTLTextureUsageShaderRead; d.storageMode = MTLStorageModeShared;
            m_texture = [m_device newTextureWithDescriptor:d];
            m_w = w; m_h = h;
        }
        return m_texture != nil;
    }
    bool send(const QImage& img, double) override {
        if (!m_server || img.width() != m_w || img.height() != m_h) return false;
        @autoreleasepool {
            [m_texture replaceRegion:MTLRegionMake2D(0, 0, NSUInteger(m_w), NSUInteger(m_h)) mipmapLevel:0 withBytes:img.constBits() bytesPerRow:NSUInteger(img.bytesPerLine())];
            id<MTLCommandBuffer> cb = [m_queue commandBuffer];
            // QImage rows are top-down, matching Metal's top-left texture origin.
            [m_server publishFrameTexture:m_texture onCommandBuffer:cb imageRegion:NSMakeRect(0, 0, m_w, m_h) flipped:NO];
            [cb commit];
        }
        return true;
    }
    void close() override {
        if (m_server) { [m_server stop]; m_server = nil; }
        m_texture = nil; m_queue = nil; m_device = nil;
    }
    int connections() override { return m_server ? ([m_server hasClients] ? 1 : 0) : -1; }
private:
    id<MTLDevice> m_device = nil; id<MTLCommandQueue> m_queue = nil; id<MTLTexture> m_texture = nil; SyphonMetalServer* m_server = nil;
    int m_w = 0, m_h = 0;
};

std::unique_ptr<VideoSink> createSyphonSink() { return std::make_unique<SyphonSink>(); }

} // namespace lf
