typedef struct tuple_f {
    union {
        struct {
            f32 x;
            f32 y;
            f32 z;
        };
        f32 f[3];
    };
} tuple_f;

typedef struct rgb {
    u8 red;
    u8 green;
    u8 blue;
} rgb;

typedef struct rgba {
    u8 red;
    u8 green;
    u8 blue;
    u8 alpha;
} rgba;
