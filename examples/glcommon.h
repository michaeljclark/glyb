#pragma once

typedef unsigned uint;

typedef struct {
    GLuint pid;
    std::map<std::string,GLuint> attrs;
    std::map<std::string,GLuint> uniforms;
} program;

typedef struct {
    GLuint tbo;
    GLuint tex;
} texture_buffer;

static std::vector<char> load_file(const char *filename) 
{ 
    std::vector<char> buf;
    FILE *f = fopen(filename, "rb");
    if (f == NULL) { 
        Error("unable to read file: %s: %s", filename, strerror(errno));
        return buf;
    } 
    fseek(f, 0, SEEK_END);
    buf.resize(ftell(f));
    fseek(f, 0, SEEK_SET);
    if (fread(buf.data(), sizeof(char), buf.size(), f) != buf.size()) { 
        Error("unable to read file: %s: short read", filename);
    } 
    fclose(f);
    return buf;
}

static GLuint compile_shader(GLenum type, const char *filename)
{
    std::vector<char> buf;
    GLint length, status;
    GLuint shader;

    buf = load_file(filename);
    length = (GLint)buf.size();
    if (!buf.size()) {
        Error("failed to load shader: %s\n", filename);
        exit(1);
    }
    shader = glCreateShader(type);

    glShaderSource(shader, (GLsizei)1, (const GLchar * const *)&buf, &length);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        buf.resize(length + 1);
        glGetShaderInfoLog(shader, (GLsizei)buf.size() - 1, &length, buf.data());
        Debug("shader compile log: %s\n", buf.data());
    }
    
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        Error("failed to compile shader: %s\n", filename);
        exit(1);
    }
    
    return shader;
}

static std::string to_string(std::map<std::string,GLuint> &list)
{
    std::string s("{");
    for (auto &ent : list) {
        s.append(s.size() > 1 ? ", ": " ");
        s.append(ent.first);
        s.append("=");
        s.append(std::to_string(ent.second));
    }
    s.append(s.size() > 1 ? " }": "}");
    return s;
}

static void link_program(program *prog, GLuint vsh, GLuint fsh,
    std::vector<std::string> attrs = std::vector<std::string>())
{
    GLuint n = 1;
    GLint status, numattrs, numuniforms;

    prog->pid = glCreateProgram();
    glAttachShader(prog->pid, vsh);
    glAttachShader(prog->pid, fsh);

    glLinkProgram(prog->pid);
    glGetProgramiv(prog->pid, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        Error("failed to link shader program: prog=%d\n", prog->pid);
        exit(1);
    }

    glGetProgramiv(prog->pid, GL_ACTIVE_ATTRIBUTES, &numattrs);
    for (GLint i = 0; i < numattrs; i++)  {
        GLint namelen=-1, size=-1;
        GLenum type = GL_ZERO;
        char namebuf[128];
        glGetActiveAttrib(prog->pid, i, sizeof(namebuf)-1, &namelen, &size,
                          &type, namebuf);
        namebuf[namelen] = 0;
        prog->attrs[namebuf] = i;
    }

    glGetProgramiv(prog->pid, GL_ACTIVE_UNIFORMS, &numuniforms);
    for (GLint i = 0; i < numuniforms; i++) {
        GLint namelen=-1, size=-1;
        GLenum type = GL_ZERO;
        char namebuf[128];
        glGetActiveUniform(prog->pid, i, sizeof(namebuf)-1, &namelen, &size,
                           &type, namebuf);
        namebuf[namelen] = 0;
        prog->uniforms[namebuf] = glGetUniformLocation(prog->pid, namebuf);
    }

    /*
     * Note: OpenGL by default binds attributes to locations counting
     * from zero upwards. This is problematic with at least the Nvidia
     * drvier, where zero has a special meaning. So after linking, we
     * go through the passed attributes and re-assign their bindings
     * starting from 1 counting upwards. We then re-link the program,
     * as we don't know the attribute names until shader is linked.
     */
    /* bind specified attributes so order is consistent across shaders */
    GLuint attr_idx = 1;
    for (auto &attr_name : attrs) {
        auto ai = prog->attrs.find(attr_name);
        if (ai != prog->attrs.end()) {
            glBindAttribLocation(prog->pid, (prog->attrs[attr_name] = attr_idx),
                attr_name.c_str());
        }
        attr_idx++;
    }
    /* bind remaining unspecified attributes in increasing order */
    for (auto &ent : prog->attrs) {
        auto ai = std::find(attrs.begin(), attrs.end(), ent.first);
        if (ai == attrs.end()) {
            glBindAttribLocation(prog->pid, (prog->attrs[ent.first] = attr_idx),
                ent.first.c_str());
            attr_idx++;
        }
    }
    glLinkProgram(prog->pid);
    glGetProgramiv(prog->pid, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        Error("failed to relink shader program: prog=%d\n", prog->pid);
        exit(1);
    }

    Debug("program = %u, attributes %s, uniforms %s\n", prog->pid,
        to_string(prog->attrs).c_str(),
        to_string(prog->uniforms).c_str());
}

static std::unique_ptr<program> make_program(GLuint vsh, GLuint fsh,
    std::vector<std::string> attrs = std::vector<std::string>())
{
    auto prog = std::make_unique<program>();
    link_program(prog.get(), vsh, fsh, attrs);
    return std::move(prog);
}

static void use_program(program *prog)
{
    glUseProgram(prog->pid);
    for (auto &ent : prog->attrs) {
        glBindAttribLocation(prog->pid, prog->attrs[ent.first],
            ent.first.c_str());
    }
}

template <typename T>
static void vertex_buffer_create(const char* name, GLuint *obj,
    GLenum target, std::vector<T> &v)
{
    if (!*obj) {
        glGenBuffers(1, obj);
        Debug("buffer %s = %u (%zu bytes)\n", name, *obj, v.size() * sizeof(T));
    }
    glBindBuffer(target, *obj);
    glBufferData(target, v.size() * sizeof(T), v.data(), GL_STATIC_DRAW);
    glBindBuffer(target, *obj);
}

template<typename X, typename T>
static void vertex_array_pointer(program *prog, const char *attr, GLint size,
    GLenum type, GLboolean norm, X T::*member)
{
    const void *obj = (const void *)reinterpret_cast<std::ptrdiff_t>(
        &(reinterpret_cast<T const *>(NULL)->*member) );
    if (prog->attrs.find(attr) != prog->attrs.end()) {
        glEnableVertexAttribArray(prog->attrs[attr]);
        glVertexAttribPointer(prog->attrs[attr], size, type, norm, sizeof(T), obj);
    }
}

static void vertex_array_1f(program *prog, const char *attr, float v1)
{
    if (prog->attrs.find(attr) != prog->attrs.end()) {
        glDisableVertexAttribArray(prog->attrs[attr]);
        glVertexAttrib1f(prog->attrs[attr], v1);
    }
}

static void vertex_array_4f(program *prog, const char *attr, float v1,
    float v2, float v3, float v4)
{
    if (prog->attrs.find(attr) != prog->attrs.end()) {
        glDisableVertexAttribArray(prog->attrs[attr]);
        glVertexAttrib4f(prog->attrs[attr], v1, v2, v3, v4);
    }
}

static void uniform_1i(program *prog, const char *uniform, GLint i)
{
    if (prog->uniforms.find(uniform) != prog->uniforms.end()) {
        glUniform1i(prog->uniforms[uniform], i);
    }
}

static void uniform_matrix_4fv(program *prog, const char *uniform, const GLfloat *mat)
{
    if (prog->uniforms.find(uniform) != prog->uniforms.end()) {
        glUniformMatrix4fv(prog->uniforms[uniform], 1, GL_FALSE, mat);
    }
}

template <typename T>
static void buffer_texture_create(texture_buffer &buf, std::vector<T> vec,
    GLenum texture, GLenum format)
{
    GLvoid *data = vec.data();
    size_t length = vec.size() * sizeof(T);
    bool created = false;

    if (!buf.tbo) {
        glGenBuffers(1, &buf.tbo);
        created = true;
    }
    glBindBuffer(GL_TEXTURE_BUFFER, buf.tbo);
    glBufferData(GL_TEXTURE_BUFFER, length, data, GL_STATIC_DRAW);

    if (!buf.tex) {
        glGenTextures(1, &buf.tex);
        created = true;
    }
    glActiveTexture(texture);
    glBindTexture(GL_TEXTURE_BUFFER, buf.tex);
    glTexBuffer(GL_TEXTURE_BUFFER, format, buf.tbo);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    if (created) {
        Debug("buffer texture unit = %zu tbo = %u, tex = %u, size = %zu\n",
            (size_t)(texture - GL_TEXTURE0), buf.tbo, buf.tex, length);
    }
}

static GLuint image_create_texture(draw_image img)
{
    static const GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};

    GLuint tex;

    GLsizei width = (GLsizei)img.size[0];
    GLsizei height = (GLsizei)img.size[1];
    GLsizei depth = (GLsizei)img.size[2];

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    if (img.flags & filter_nearest) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    if (img.flags & filter_linear) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    switch (depth) {
    case 1:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height,
            0, GL_RED, GL_UNSIGNED_BYTE, (GLvoid*)img.pixels);
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        break;
    case 4:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)img.pixels);
        break;
    }
    glActiveTexture(GL_TEXTURE0);

    Debug("image %u = %u x %u x %u\n", tex, width, height, depth);

    return tex;
}

static void image_update_texture(GLuint tex, draw_image img)
{
    GLsizei width = (GLsizei)img.size[0];
    GLsizei height = (GLsizei)img.size[1];
    GLsizei depth = (GLsizei)img.size[2];

    /* skip texture update if modified width and height is less than zero
     * note: we currently ignore the dimensions of the modified rectangle */
    if (img.modrect[2] <= 0 || img.modrect[3] <= 0) return;

    glBindTexture(GL_TEXTURE_2D, tex);

    switch (depth) {
    case 1:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height,
            0, GL_RED, GL_UNSIGNED_BYTE, (GLvoid*)img.pixels);
        break;
    case 4:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)img.pixels);
        break;
    }
}

static GLenum cmd_mode_gl(int cmd_mode)
{
    switch (cmd_mode) {
    case mode_lines:     return GL_LINES;
    case mode_triangles: return GL_TRIANGLES;
    default: return GL_NONE;
    }
}