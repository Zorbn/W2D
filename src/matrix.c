#include "matrix.h"

void orthographicProjection(float matrix[matrix4Components], float left,
                            float right, float bottom, float top, float zNear,
                            float zFar) {
    matrix[0] = 2.0f / (right - left);
    matrix[1] = 0;
    matrix[2] = 0;
    matrix[3] = 0;

    matrix[4] = 0;
    matrix[5] = 2.0f / (top - bottom);
    matrix[6] = 0;
    matrix[7] = 0;

    matrix[8] = 0;
    matrix[9] = 0;
    matrix[10] = 1.0f / (zNear - zFar);
    matrix[11] = 0;

    matrix[12] = (right + left) / (left - right);
    matrix[13] = (top + bottom) / (bottom - top);
    matrix[14] = zNear / (zNear - zFar);
    matrix[15] = 1.0f;
}