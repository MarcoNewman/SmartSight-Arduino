#ifndef MODEL_SETTINGS_H_
#define MODEL_SETTINGS_H_

// Keeping these as constant expressions allow us to allocate fixed-sized arrays
// on the stack for our working memory.

// MEDIAPIPE SETTINGS
// input
constexpr int kCols_mediapipe = 128;
constexpr int kRows_mediapipe = 128;
constexpr int kChannels_mediapipe = 3;
constexpr int kImageSize_mediapipe = kCols_mediapipe * kRows_mediapipe * kChannels_mediapipe;

// output
constexpr int kBoxes_mediapipe = 896;
constexpr int kAnchorsSize_mediapipe = kBoxes_mediapipe * 2;
extern const float kAnchors_mediapipe[kAnchorsSize_mediapipe];


// MOBILENET
constexpr int kCategoryCount = 2;
constexpr int kPersonIndex = 1;
constexpr int kNotAPersonIndex = 0;
extern const char* kCategoryLabels[kCategoryCount];

#endif // MODEL_SETTINGS_H_
