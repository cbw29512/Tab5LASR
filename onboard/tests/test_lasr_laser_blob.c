#include "../main/lasr_laser_pipeline.h"
#include "../main/lasr_csi_format.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <linux/videodev2.h>

#define W 64u
#define H 48u
#define STRIDE (W*2u+8u)
static uint8_t current[H*STRIDE],baseline[H*STRIDE];
static lasr_blob_config_t cfg(void) {
    return (lasr_blob_config_t){.left=.1f,.top=.1f,.right=.9f,.bottom=.9f,
        .min_luma=160,.min_delta=100,.step=1,.min_cluster=3,
        .radius_px=4,.max_candidates=70};
}
static void point(uint8_t *bytes,uint32_t x,uint32_t y, bool big_endian) {
    const size_t p=(size_t)y*STRIDE+x*2;
    // RGB565 white 0xffff, no dependence on display Color 2.
    if (big_endian) {bytes[p]=0xff;bytes[p+1]=0xff;}
    else {bytes[p]=0xff;bytes[p+1]=0xff;}
}
static void spot(uint8_t *bytes,uint32_t x,uint32_t y) {
    for(uint32_t dy=0;dy<3;dy++)for(uint32_t dx=0;dx<3;dx++)
        point(bytes,x+dx,y+dy,false);
}
static lasr_raw_frame_t frame(const uint8_t *data,lasr_raw_format_t kind) {
    return (lasr_raw_frame_t){data,sizeof(current),STRIDE,W,H,kind};
}
static bool step(lasr_laser_pipeline_t *p,uint64_t us, bool expect) {
    lasr_raw_frame_t a=frame(current,LASR_RAW_RGB565_LE);
    lasr_raw_frame_t b=frame(baseline,LASR_RAW_RGB565_LE);
    lasr_pulse_event_t event={0};
    assert(lasr_laser_pipeline_step(p,&a,&b,us,&event));
    assert(event.shot_detected==expect);
    if(expect) {
        assert(fabsf(event.x_norm-16.5f/W)<.02f);
        assert(fabsf(event.y_norm-22.5f/H)<.02f);
    }
    return event.shot_detected;
}
int main(void) {
    lasr_raw_format_t pixel_format;
    assert(lasr_csi_format_map(V4L2_PIX_FMT_RGB565,&pixel_format));
    assert(pixel_format==LASR_RAW_RGB565_LE);
    assert(lasr_csi_format_map(V4L2_PIX_FMT_RGB565X,&pixel_format));
    assert(pixel_format==LASR_RAW_RGB565_BE);
    assert(lasr_csi_format_map(V4L2_PIX_FMT_UYVY,&pixel_format));
    assert(pixel_format==LASR_RAW_UYVY);
    assert(!lasr_csi_format_map(V4L2_PIX_FMT_JPEG,&pixel_format));
    uint8_t luma=0;
    lasr_raw_frame_t a=frame(current,LASR_RAW_RGB565_LE);
    assert(lasr_raw_luma(&a,16,22,&luma)&&luma==0);
    assert(lasr_raw_luma_validated(&a,16,22)==luma);
    assert(!lasr_raw_luma(&a,W,1,&luma));
    a.length=H*STRIDE-1;assert(!lasr_raw_frame_valid(&a));
    a=frame(current,(lasr_raw_format_t)-1);assert(!lasr_raw_frame_valid(&a));
    a=frame(current,LASR_RAW_RGB565_LE);
    lasr_raw_frame_t b=frame(baseline,LASR_RAW_RGB565_LE);
    lasr_blob_result_t found;
    assert(lasr_blob_scan(&a,&b,&(lasr_blob_config_t){.left=0,.top=0,.right=1,
      .bottom=1,.min_luma=120,.min_delta=40,.step=0,.min_cluster=1,
      .radius_px=3,.max_candidates=100},&found)==false);
    const lasr_blob_config_t config=cfg();
    // A static bright lamp appears in the confirmed baseline: not a laser flash.
    spot(baseline,15,21);spot(current,15,21);
    assert(lasr_blob_scan(&a,&b,&config,&found));assert(!found.visible);
    memset(current,0,sizeof(current));memset(baseline,0,sizeof(baseline));
    lasr_laser_pipeline_t p;
    assert(lasr_laser_pipeline_init(&p,cfg(),(lasr_pulse_config_t){2,2,10000}));
    step(&p,1000,false);step(&p,2000,false); // Must see darkness first.
    spot(current,15,21);
    step(&p,3000,false);step(&p,4000,true);
    step(&p,5000,false);step(&p,6000,false); // Held laser counts ONCE.
    memset(current,0,sizeof(current));
    step(&p,7000,false);step(&p,8000,false);
    spot(current,15,21);
    step(&p,9000,false);step(&p,10000,true); // Re-arm after dark.

    // A long interval is normal when camera capture stalls temporarily.
    // Scanner reports the frame as processed, but discards the old pulse
    // and requires fresh dark frames before detecting another hit.
    memset(current,0,sizeof(current));
    step(&p,40000,false); // 30ms exceeds the configured 10ms max gap.
    spot(current,15,21);
    step(&p,41000,false); // Cannot count a laser already visible after gap.
    memset(current,0,sizeof(current));
    step(&p,42000,false);step(&p,43000,false);
    spot(current,15,21);
    step(&p,44000,false);step(&p,45000,true);

    memset(current,0xff,sizeof(current));
    assert(lasr_blob_scan(&a,&b,&config,&found)&&!found.visible);
    // Combined hardware probe: a 3x3 pulse must survive all three
    // coarse-grid phases, and the dense neighborhood must recover center.
    lasr_blob_config_t fast=cfg();
    fast.step=3;
    for (uint32_t phase=0;phase<3;++phase) {
        memset(current,0,sizeof(current));
        spot(current,15+phase,21+phase);
        assert(lasr_blob_scan(&a,&b,&fast,&found));
        assert(found.visible && found.cluster>=3);
        assert(fabsf(found.x_norm-(16.5f+phase)/W)<.02f);
        assert(fabsf(found.y_norm-(22.5f+phase)/H)<.02f);
    }
    memset(current,0,sizeof(current));
    point(current,18,25,false);
    assert(lasr_blob_scan(&a,&b,&fast,&found)&&!found.visible);
    memset(current,0xff,sizeof(current));
    assert(lasr_blob_scan(&a,&b,&fast,&found)&&!found.visible);
    // UYVY luma at byte +1; deliberate RGB565X byte order handling.
    memset(current,0,sizeof(current));
    current[5*STRIDE+2*6]=0x80;current[5*STRIDE+2*6+1]=200;
    a=frame(current,LASR_RAW_UYVY);
    assert(lasr_raw_luma(&a,6,5,&luma)&&luma==200);
    assert(lasr_raw_luma_validated(&a,6,5)==luma);
    current[5*STRIDE+2*6]=0xf8;current[5*STRIDE+2*6+1]=0;
    a=frame(current,LASR_RAW_RGB565_BE);
    assert(lasr_raw_luma(&a,6,5,&luma)&&luma>40);
    assert(lasr_raw_luma_validated(&a,6,5)==luma);
    a=frame(current,LASR_RAW_RGB565_LE);
    assert(lasr_raw_luma(&a,6,5,&luma)&&luma<40);
    assert(lasr_raw_luma_validated(&a,6,5)==luma);
    puts("LASR raw CSI scanner/pulse: formats, stride, ROI, hold/rearm, broad-light rejection passed");
}