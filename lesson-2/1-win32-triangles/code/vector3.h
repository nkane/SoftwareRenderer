typedef struct _vector3f
{
    union 
    {
        float data[3];
        struct 
        {
            real32 X;
            real32 Y;
            real32 Z;
        };
    };
} vector3f;

inline vector3f
Subtract_Vector3f(vector3f v1, vector3f v2)
{
    vector3f result;
    result.X = v1.X - v2.X;
    result.Y = v1.Y - v2.Y;
    result.Z = v1.Z - v2.Z;
    return result;
}

inline vector3f
Add_Vector3f(vector3f v1, vector3f v2)
{
    vector3f result;
    result.X = v1.X + v2.X;
    result.Y = v1.Y + v2.Y;
    result.Z = v1.Z + v2.Z;
    return result;
}

inline real32
Dot_Product_2D_Vector3f(vector3f v1, vector3f v2)
{
    return (v1.X * v2.X) + (v1.Y * v2.Y);
}

inline real32
Cross_Product_2D_Vector3f(vector3f v1, vector3f v2)
{
    return (v1.X * v2.Y) + (v1.Y * v2.X);
}

typedef struct _vector3i
{
    union 
    {
        int32 data[3];
        struct 
        {
            int32 X;
            int32 Y;
            int32 Z;
        };
        struct 
        {
            int32 Red;
            int32 Green;
            int32 Blue;
        };
    };
} vector3i;