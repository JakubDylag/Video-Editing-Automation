/**
 * @file Clip.h
 * @author Devon Crawford
 * @date February 13, 2019
 * @brief File containing the definition and usage for Clip API:
 * Clip stores a reference to a video file and its data within an editing sequence
 */
#ifndef _CLIP_API_
#define _CLIP_API_

#include "VideoContext.h"
#include "Timebase.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/**
    Clip stores a reference to a video file and its data within an editing sequence.
    This way, we can access the AVPackets whenever needed, and further decode or
    process the data
*/
typedef struct Clip {
    /********** ORIGINAL FILE DATA **********/
    /*
        Original file on disk, represented by VideoContext.
        This should be allocated with open_video() before being set here..
    */
    VideoContext *vid_ctx;
    /*
        pts on original packets to find start and end frames.
        for videos.. this is the pts of video stream, for audio
        its the pts of audio stream.
        You can use cov_video_to_audio_pts() in Timebase.c to convert these
        video pts into the equivalent audio pts!
        IMPORTANT:
        time_base: same as VideoContext video_stream time_base
    */
    int64_t orig_start_pts, orig_end_pts;

    /*
        pts of seek, absolute to original video pts
        time_base is the same as VideoContext video_stream time_base
        RANGE:
        >= orig_start_pts and <= orig_end_pts
     */
    int64_t seek_pts;

    /*
        filename
    */
    char *url;
    /*
        video context open or closed state
    */
    bool open;
    /*
        Current location of the seek pointer within the original file data.
        By default this is start_frame_pts, but will change when reading packets.
        (We track this by video packets seen and seek usage)
     */
    int64_t current_frame_idx;

    /********** EDIT SEQUENCE DATA **********/
    /*
        Position of clip in the edit sequence!! (pts of first/last packet in clip)
        These positions are INCLUSIVE.
        First packet.pts is start_pts while last packet.pts is end_pts
        IMPORTANT:
        time_base: same as sequence time_base
        by default, end_pts will be automatically set to size by orig_end_pts in move_clip()
    */
    int64_t start_pts, end_pts;

    /********** INTERNAL ONLY **********/
    // DO NOT USE, YOU WILL BREAK SOME INTERNAL FUNCTIONS (clip_read_packet)
    bool done_reading_video, done_reading_audio;
} Clip;

/**
    Initialize Clip object to full length of original video file
    Return >= 0 on success
*/
int init_clip(Clip *clip, char *url);

/*
    Open a clip (VideoContext) to read data from the original file
    Return >= 0 if OK, < 0 on fail
*/
int open_clip(Clip *clip);

int open_clip_bounds(Clip *clip, int64_t start_idx, int64_t end_idx);

void close_clip(Clip *clip);

/**
    Initialize Clip object with start and endpoints
    @param clip: Clip to initialize
    @param vid_ctx: Video context assumed to already be initialized
    @param start_idx: start frame number in original video file
    @param end_idx: end frame number in original video file
    Return >= 0 on success
*/
int set_clip_bounds(Clip *clip, int64_t start_idx, int64_t end_idx);

/**
    Return >= 0 on success
*/
int set_clip_start_frame(Clip *clip, int64_t frameIndex);

/**
    Return >= 0 on success
*/
int set_clip_start(Clip *clip, int64_t pts);

/**
    Return >= 0 on success
*/
int set_clip_end_frame(Clip *clip, int64_t frameIndex);

/**
    Return >= 0 on success
*/
int set_clip_end(Clip *clip, int64_t pts);

/**
    Seek to a clip frame, relative to the clip.
    @param Clip: clip to seek
    @param seekFrameIndex: frame index within the clip to seek
*/
int seek_clip(Clip *clip, int64_t seekFrameIndex);

/**
    Seek to a clip frame, relative to the clip.
    @param Clip: clip to seek
    @param pts: relative pts to the clip (representing the frame to seek)
*/
int seek_clip_pts(Clip *clip, int64_t pts);

/**
 * Gets absolute pts for clip (the pts of original video file)
 * @param  clip         Clip
 * @param  relative_pts pts relative to clip (where zero represents clip->orig_start_pts)
 * @return              absolute pts of clip. (relative to original video file)
 */
int64_t get_abs_clip_pts(Clip *clip, int64_t relative_pts);

/**
 * Convert absolute pts (from VideoContext), into a pts relative to the Clip bounds
 * @param  clip    Clips
 * @param  abs_pts absolute pts (relative to original video)
 * @return         abs_pts - clip->orig_start_pts
 *                 >= 0 if abs_pts is within start boundary
 */
int64_t cov_clip_pts_relative(Clip *clip, int64_t abs_pts);

/**
 * Convert raw video packet timestamp into clip relative pts
 * @param  clip    Clip
 * @param  pkt_pts raw packet pts from original video file
 * @return         pts relative to clip video
 */
int64_t clip_ts_video(Clip *clip, int64_t pkt_ts);

/**
 * Convert raw video packet timestamp into clip relative audio pts
 * @param  clip    Clip
 * @param  pkt_pts raw packet pts from original video file (in video timebase)
 * @return         pts relative to clip audio timebase
 */
int64_t clip_ts_audio(Clip *clip, int64_t pkt_ts);

/**
    Gets index of last frame in clip
    @param clip: Clip to get end frame
    Return >= 0 on success
*/
int64_t get_clip_end_frame_idx(Clip* clip);

/**
 * Detects if we are done reading the current packet stream..
 * if true.. then the packet in parameter should be skipped over!
 * @param  clip clip
 * @param  pkt  AVPacket currently read
 * @return      true, if our clip has read the entire stream associated with this packet.
 *              false, otherwise
 */
bool done_curr_pkt_stream(Clip *clip, AVPacket *pkt);

/**
    Read a single AVPacket from clip.
    Only returns packets if they are within clip boundaries (start_frame_pts - end_frame_pts)
    This is a wrapper on av_read_frame() from ffmpeg. It will increment the packet
    counter by 1 on each call. The idea is to call this function in a loop while return >= 0.
    When end of clip is reached the packet pointer will be reset to start of clip (seek 0)
    @param Clip to read packets
    @param pkt: output AVPacket when valid packet is read.. otherwise NULL
    Returns >= 0 on success. < 0 when reached EOF, end of clip boundary or error.
    AVPacket is returned in param pkt.
*/
int clip_read_packet(Clip *clip, AVPacket *pkt);

/**
 * Reset packet counter to beginning of clip and reset internal reading flags (to allow another read cycle)
 * @param  clip Clip
 * @return      >= 0 on success
 */
int reset_packet_counter(Clip *clip);

/**
 * Set internal vars used in reading stream data
 * @param clip Clip
 */
void init_internal_vars(Clip *clip);

/**
 * Compare two clips based on pts
 * @param  first  First clip to compare
 * @param  second Second clip to compare
 * @return        > 0 if first->pts > second->pts.  (first after second)
 *                < 0 if first->pts < second->pts.  (second after first)
 *                  0 if first->pts = second->pts.  (first and second at same pts)
 */
int64_t compare_clips(Clip *first, Clip *second);

/**
 * Get time_base of clip video stream
 * @param  clip Clip
 * @return      time_base of clip video stream
 */
AVRational get_clip_video_time_base(Clip *clip);

/**
 * Get time_base of clip audio stream
 * @param  clip Clip
 * @return      time_base of clip audio stream
 */
AVRational get_clip_audio_time_base(Clip *clip);

/**
 * Get video stream from clip
 * @param  clip Clip containing VideoContext
 * @return      video AVStream on success, NULL on failure
 */
AVStream *get_clip_video_stream(Clip *clip);

/**
 * Get audio stream from clip
 * @param  clip Clip containing VideoContext
 * @return      audio AVStream on success, NULL on failure
 */
AVStream *get_clip_audio_stream(Clip *clip);

/**
 * Get codec parameters of clip video stream
 * @param  clip Clip to get parameters
 * @return      not NULL on success
 */
AVCodecParameters *get_clip_video_params(Clip *clip);

/**
 * Get codec parameters of clip audio stream
 * @param  clip Clip to get parameters
 * @return      not NULL on success
 */
AVCodecParameters *get_clip_audio_params(Clip *clip);

/**
    Frees data within clip structure (does not free Clip allocation itself)
*/
void free_clip(Clip *clip);

/*************** LINKED LIST FUNCTIONS ***************/
/**
 * Get data about a clip in a string
 * @param  toBePrinted Clip containing data
 * @return             data in string
 */
char *list_print_clip(void *toBePrinted);

/**
 * Free clip memory and all its data. Assumes clip was allocated on heap.
 * @param toBeDeleted Clip structure allocated on heap
 */
void list_delete_clip(void *toBeDeleted);

/**
 * Compare two clips within a linked list (uses compare_clips() internally)
 * @param  first  first clip to compare
 * @param  second second clip to compare
 * @return        > 0 if first->pts > second->pts.  (first after second)
 *                < 0 if first->pts < second->pts.  (second after first)
 *                  0 if first->pts = second->pts.  (first and second at same pts)
 */
int list_compare_clips(const void *first, const void *second);

/**
 * Test example showing how to read packets from clips
 * @param clip Clip
 */
void example_clip_read_packets(Clip *clip);

#endif