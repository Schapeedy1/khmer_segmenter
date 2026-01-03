#ifndef KHMER_SEGMENTER_H
#define KHMER_SEGMENTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KhmerSegmenter KhmerSegmenter;

/**
 * @brief Initialize the Khmer Segmenter.
 * 
 * @param dictionary_path Path to the dictionary file (line-separated words).
 * @param frequency_path Path to the frequency file (JSON or text).
 * @return Pointer to the segmenter instance, or NULL on failure.
 */
KhmerSegmenter* khmer_segmenter_init(const char* dictionary_path, const char* frequency_path);

/**
 * @brief Segment a Khmer string.
 * 
 * @param segmenter Pointer to the segmenter instance.
 * @param text UTF-8 encoded Khmer text to segment.
 * @return A string containing the segmented text with zero-width spaces (or custom separator) inserted.
 *         The caller is responsible for freeing the returned string.
 */
char* khmer_segmenter_segment(KhmerSegmenter* segmenter, const char* text, const char* separator);

/**
 * @brief Free the segmenter instance.
 * 
 * @param segmenter Pointer to the segmenter instance.
 */
void khmer_segmenter_free(KhmerSegmenter* segmenter);

#ifdef __cplusplus
}
#endif

#endif // KHMER_SEGMENTER_H
