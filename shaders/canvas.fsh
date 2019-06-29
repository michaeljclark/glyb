// ported from msdfgen

#version 140

varying vec4 v_color;
varying vec2 v_uv0;
varying float v_gamma;
varying float v_glyph;

uniform isamplerBuffer u_tex0;
uniform isamplerBuffer u_tex1;
uniform samplerBuffer u_tex2;

#define FLT_MAX 3.4028235e38
#define M_PI 3.1415926535897932384626433832795

#define Linear 2
#define Quadratic 3
#define Cubic 4

struct Shape {
    int contour_offset, contour_count, edge_offset, edge_count;
    vec2 offset, size;
};

struct Edge {
    int edge_type;
    vec2 p[4];
};

int solveQuadratic(out vec3 x, float a, float b, float c)
{
    if (abs(a) < 1e-14) {
        if (abs(b) < 1e-14) {
            if (c == 0)
                return -1;
            return 0;
        }
        x[0] = -c/b;
        return 1;
    }
    float dscr = b*b-4*a*c;
    if (dscr > 0) {
        dscr = sqrt(dscr);
        x[0] = (-b+dscr)/(2*a);
        x[1] = (-b-dscr)/(2*a);
        return 2;
    } else if (dscr == 0) {
        x[0] = -b/(2*a);
        return 1;
    } else
        return 0;
}

int solveCubicNormed(out vec3 x, float a, float b, float c)
{
    float a2 = a*a;
    float q  = (a2 - 3*b)/9;
    float r  = (a*(2*a2-9*b) + 27*c)/54;
    float r2 = r*r;
    float q3 = q*q*q;
    float A, B;
    if (r2 < q3) {
        float t = r/sqrt(q3);
        if (t < -1) t = -1;
        if (t > 1) t = 1;
        t = acos(t);
        a /= 3; q = -2*sqrt(q);
        x[0] = q*cos(t/3)-a;
        x[1] = q*cos((t+2*M_PI)/3)-a;
        x[2] = q*cos((t-2*M_PI)/3)-a;
        return 3;
    } else {
        A = -pow(abs(r)+sqrt(r2-q3), 1/3.);
        if (r < 0) A = -A;
        B = A == 0 ? 0 : q/A;
        a /= 3;
        x[0] = (A+B)-a;
        x[1] = -0.5*(A+B)-a;
        x[2] = 0.5*sqrt(3.)*(A-B);
        if (abs(x[2]) < 1e-14) {
            return 2;
        }
        return 1;
    }
}

int solveCubic(out vec3 x, float a, float b, float c, float d)
{
    if (abs(a) < 1e-14) {
        return solveQuadratic(x, b, c, d);
    } else {
        return solveCubicNormed(x, b/a, c/a, d/a);
    }
}

vec2 directionQuadratic(vec2 p[4], float param)
{
    vec2 tangent = mix(p[1]-p[0], p[2]-p[1], param);
    if (tangent.x == 0 && tangent.y == 0) {
        return p[2]-p[0];
    }
    return tangent;
}

vec2 directionCubic(vec2 p[4], float param)
{
    vec2 tangent = mix(mix(p[1]-p[0], p[2]-p[1], param), mix(p[2]-p[1], p[3]-p[2], param), param);
    if (tangent.x == 0 && tangent.y == 0) {
        if (param == 0) return p[2]-p[0];
        if (param == 1) return p[3]-p[1];
    }
    return tangent;
}

float cross(vec2 a, vec2 b) {
    return a.x*b.y-a.y*b.x;
}

int nonZeroSign(float p) { return p > 0 ? 1 : -1; }
int nonZeroSign(vec2 p) { return length(p) > 0 ? 1 : -1; }
int nonZeroSign(vec3 p) { return length(p) > 0 ? 1 : -1; }

vec2 getOrthonormal(vec2 p, bool polarity, bool allowZero)
{
    float len = length(p);
    if (len == 0) {
        return polarity ? vec2(0, allowZero ? 0 : 1) : vec2(0, allowZero ? 0 : -1);
    } else {
        return polarity ? vec2(-p.y/len, p.x/len) : vec2(p.y/len, -p.x/len);
    }
}

float signedDistanceLinear(vec2 p[4], out float dir, vec2 origin, out float param)
{
    vec2 aq = origin-p[0];
    vec2 ab = p[1]-p[0];
    param = dot(aq, ab)/dot(ab, ab);
    vec2 eq = p[param > .5 ? 1 : 0]-origin;
    float endpointDistance = length(eq);
    if (param > 0 && param < 1) {
        float orthoDistance = dot(getOrthonormal(ab, false, false), aq);
        if (abs(orthoDistance) < endpointDistance) {
            dir = 0;
            return orthoDistance;
        }
    }
    dir = abs(dot(normalize(ab), normalize(eq)));
    return nonZeroSign(cross(aq, ab))*endpointDistance;
}

float signedDistanceQuadratic(vec2 p[4], out float dir, vec2 origin, out float param)
{
    vec2 qa = p[0]-origin;
    vec2 ab = p[1]-p[0];
    vec2 br = p[2]-p[1]-ab;
    float a = dot(br, br);
    float b = 3*dot(ab, br);
    float c = 2*dot(ab, ab)+dot(qa, br);
    float d = dot(qa, ab);
    vec3 t;
    int solutions = solveCubic(t, a, b, c, d);

    vec2 epDir = directionQuadratic(p, 0);
    float minDistance = nonZeroSign(cross(epDir, qa))*length(qa); // distance from A
    param = -dot(qa, epDir)/dot(epDir, epDir);
    {
        epDir = directionQuadratic(p, 1);
        float distance = nonZeroSign(cross(epDir, p[2]-origin))*length(p[2]-origin); // distance from B
        if (abs(distance) < abs(minDistance)) {
            minDistance = distance;
            param = dot(origin-p[1], epDir)/dot(epDir, epDir);
        }
    }
    for (int i = 0; i < solutions; ++i) {
        if (t[i] > 0 && t[i] < 1) {
            vec2 qe = p[0]+2*t[i]*ab+t[i]*t[i]*br-origin;
            float distance = nonZeroSign(cross(p[2]-p[0], qe))*length(qe);
            if (abs(distance) <= abs(minDistance)) {
                minDistance = distance;
                param = t[i];
            }
        }
    }

    if (param >= 0 && param <= 1) {
        dir = 0;
        return minDistance;
    }
    if (param < .5) {
        dir = abs(dot(normalize(directionQuadratic(p, 0)), normalize(qa)));
        return minDistance;
    }
    else {
        dir = abs(dot(normalize(directionQuadratic(p, 1)), normalize(p[2]-origin)));
        return minDistance;
    }
}

float signedDistanceCubic(vec2 p[4], out float dir, vec2 origin, out float param)
{
    vec2 qa = p[0]-origin;
    vec2 ab = p[1]-p[0];
    vec2 br = p[2]-p[1]-ab;
    vec2 as = p[3]-p[2]-p[2]-p[1]-br;

    vec2 epDir = directionCubic(p, 0);
    float minDistance = nonZeroSign(cross(epDir, qa))*length(qa); // distance from A
    param = -dot(qa, epDir)/dot(epDir, epDir);
    {
        epDir = directionCubic(p, 1);
        float distance = nonZeroSign(cross(epDir, p[3]-origin))*length(p[3]-origin); // distance from B
        if (abs(distance) < abs(minDistance)) {
            minDistance = distance;
            param = dot(epDir-(p[3]-origin), epDir)/dot(epDir, epDir);
        }
    }
    // Iterative minimum distance search
    for (int i = 0; i <= 4; ++i) {
        float t = i/4;
        for (int step = 0;; ++step) {
            vec2 qe = p[0]+3*t*ab+3*t*t*br+t*t*t*as-origin; // do not simplify with qa !!!
            float distance = nonZeroSign(cross(directionCubic(p, t), qe))*length(qe);
            if (abs(distance) < abs(minDistance)) {
                minDistance = distance;
                param = t;
            }
            if (step == 4) {
                break;
            }
            // Improve t
            vec2 d1 = 3*as*t*t+6*br*t+3*ab;
            vec2 d2 = 6*as*t+6*br;
            t -= dot(qe, d1)/(dot(d1, d1)+dot(qe, d2));
            if (t < 0 || t > 1) {
                break;
            }
        }
    }

    if (param >= 0 && param <= 1) {
        dir = 0;
        return minDistance;
    }
    if (param < .5) {
        dir = abs(dot(normalize(directionCubic(p, 0)), normalize(qa)));
        return minDistance;
    }
    else {
        dir = abs(dot(normalize(directionCubic(p, 1)), normalize(p[3]-origin)));
        return minDistance;
    }
}

void getShape(out Shape shape, int shape_num)
{
    int o = shape_num * 8;
    shape.contour_offset = texelFetch(u_tex0, o + 0).r;
    shape.contour_count = texelFetch(u_tex0, o + 1).r;
    shape.edge_offset = texelFetch(u_tex0, o + 2).r;
    shape.edge_count = texelFetch(u_tex0, o + 3).r;
    shape.offset = vec2(texelFetch(u_tex0, o + 4).r,
                        texelFetch(u_tex0, o + 5).r) / 64.f;
    shape.size = vec2(texelFetch(u_tex0, o + 6).r,
                      texelFetch(u_tex0, o + 7).r) / 64.f;
}

void getEdge(out Edge edge, int edge_num)
{
    int o = edge_num * 9;
    edge.edge_type = int(texelFetch(u_tex2, o + 0).r);
    edge.p[0] = vec2(texelFetch(u_tex2, o + 1).r, texelFetch(u_tex2, o + 2).r);
    edge.p[1] = vec2(texelFetch(u_tex2, o + 3).r, texelFetch(u_tex2, o + 4).r);
    edge.p[2] = vec2(texelFetch(u_tex2, o + 5).r, texelFetch(u_tex2, o + 6).r);
    edge.p[3] = vec2(texelFetch(u_tex2, o + 7).r, texelFetch(u_tex2, o + 8).r);
}

float getDistanceEdge(Edge edge, vec2 origin, out float dir, out float param)
{
    switch(edge.edge_type) {
    case Linear:    return signedDistanceLinear   (edge.p, dir, origin, param);
    case Quadratic: return signedDistanceQuadratic(edge.p, dir, origin, param);
    case Cubic:     return signedDistanceCubic    (edge.p, dir, origin, param);
    }
    return FLT_MAX; /* not reached */
}

float getDistanceShape(Shape shape, vec2 origin, out float dir, out float param)
{
    Edge edge;
    float distance, minDistance = FLT_MAX, minDir = FLT_MAX;
    for (int i = 0; i < shape.edge_count; i++) {
        getEdge(edge, shape.edge_offset + i);
        distance = getDistanceEdge(edge, origin, dir, param);
        if (abs(distance) < abs(minDistance) || abs(distance) == abs(minDistance) && dir < minDir) {
            minDistance = distance;
            minDir = dir;
        }
    }
    return minDistance;
}

mat3 getShapeTransform(Shape shape)
{
    const float padding = 8;
    vec2 size = vec2(shape.size.x, shape.size.y) + padding;
    float scale = max(size.x,size.y);
    vec2 rem = vec2(shape.size.x, shape.size.y) - vec2(scale);
    return mat3(
        scale,       0,       0 + rem.x/2 + shape.offset.x/64.0f ,
        0,      -scale,   scale + rem.y/2 + shape.offset.y/64.0f ,
        0,           0,   scale
    );
}

void main()
{
    Shape shape;
    getShape(shape, 0);

    mat3 transform = getShapeTransform(shape);
    vec3 p = vec3(v_uv0.xy, 1) * transform;

    float dir, param;
    float distance = getDistanceShape(shape, p.xy, dir, param);

    float dx = dFdx( v_uv0.x );
    float dy = dFdy( v_uv0.y );
    float m = sqrt(shape.size.x * shape.size.y) * sqrt(dx*dx + dy*dy);

    float alpha = smoothstep(-m, m, distance);

    gl_FragColor = vec4(v_color.rgb,alpha);
}
