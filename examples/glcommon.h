#pragma once

typedef unsigned uint;

typedef struct {
    GLuint pid;
    std::map<std::string,GLuint> attrs;
    std::map<std::string,GLuint> uniforms;
} program;

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

static void link_program(program *prog, GLuint vsh, GLuint fsh)
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
    for (auto &ent : prog->attrs) {
        glBindAttribLocation(prog->pid, (prog->attrs[ent.first] = n++),
            ent.first.c_str());
    }
    glLinkProgram(prog->pid);
    glGetProgramiv(prog->pid, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        Error("failed to relink shader program: prog=%d\n", prog->pid);
        exit(1);
    }

    for (auto &ent : prog->attrs) {
        Debug("attr %s = %d\n", ent.first.c_str(), ent.second);
    }
    for (auto &ent : prog->uniforms) {
        Debug("uniform %s = %d\n", ent.first.c_str(), ent.second);
    }
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

static void image_create_texture(GLuint *tex, draw_image img)
{
    static const GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};

    GLsizei width = (GLsizei)img.size[0];
    GLsizei height = (GLsizei)img.size[1];
    GLsizei depth = (GLsizei)img.size[2];

    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);

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

    Debug("image %u = %u x %u x %u\n", *tex, width, height, depth);
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