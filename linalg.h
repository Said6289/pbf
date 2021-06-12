static float
Clamp(float Low, float Value, float High)
{
    if (Value < Low) return Low;
    if (Value > High) return High;
    return Value;
}

struct v2 {
    float x;
    float y;
};

static v2
V2(float x, float y)
{
    v2 Result = {};
    Result.x = x;
    Result.y = y;
    return Result;
}

static v2
V2(float S)
{
    v2 Result = {};
    Result.x = S;
    Result.y = S;
    return Result;
}

static v2
operator+(v2 A, v2 B)
{
    v2 Result = {};
    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    return Result;
}

static v2
operator-(v2 A, v2 B)
{
    v2 Result = {};
    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    return Result;
}

static v2
operator*(v2 A, v2 B)
{
    v2 Result = {};
    Result.x = A.x * B.x;
    Result.y = A.y * B.y;
    return Result;
}

static v2
operator*(v2 V, float S)
{
    V.x *= S;
    V.y *= S;
    return V;
}

static v2
operator*(float S, v2 V)
{
    return V * S;
}

static void
operator+=(v2 &A, v2 B)
{
    A = A + B;
}

static void
operator-=(v2 &A, v2 B)
{
    A = A - B;
}

static v2
operator-(v2 V)
{
    v2 Result = {};
    Result.x = -V.x;
    Result.y = -V.y;
    return Result;
}

static void
operator*=(v2 &A, v2 B)
{
    A = A * B;
}

static void
operator*=(v2 &V, float S)
{
    V = V * S;
}

static float
Dot(v2 A, v2 B)
{
    float Result = A.x * B.x + A.y * B.y;
    return Result;
}

static float
LengthSq(v2 V)
{
    float Result = Dot(V, V);
    return Result;
}

static float
Length(v2 V)
{
    float Result = sqrtf(LengthSq(V));
    return Result;
}

static v2
Normalize(v2 V)
{
    float Len = Length(V);
    v2 Result = {};
    Result.x = V.x / Len;
    Result.y = V.y / Len;
    return Result;
}

