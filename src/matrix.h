#ifndef MATRIX_H
#define MATRIX_H

#define matrix4Components 16

void orthographicProjection(float matrix[matrix4Components], float left,
                            float right, float bottom, float top, float zNear,
                            float zFar);

#endif