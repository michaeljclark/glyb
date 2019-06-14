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
        printf("unable to read file: %s: %s", filename, strerror(errno));
        return buf;
    } 
    fseek(f, 0, SEEK_END);
    buf.resize(ftell(f));
    fseek(f, 0, SEEK_SET);
    if (fread(buf.data(), sizeof(char), buf.size(), f) != buf.size()) { 
        printf("unable to read file: %s: short read", filename);
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
        printf("failed to load shader: %s\n", filename);
        exit(1);
    }
    shader = glCreateShader(type);

    glShaderSource(shader, (GLsizei)1, (const GLchar * const *)&buf, &length);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        buf.resize(length + 1);
        glGetShaderInfoLog(shader, (GLsizei)buf.size() - 1, &length, buf.data());
        printf("shader compile log: %s\n", buf.data());
    }
    
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        printf("failed to compile shader: %s\n", filename);
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
        printf("failed to link shader program\n");
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
        printf("failed to relink shader program\n");
        exit(1);
    }

    for (auto &ent : prog->attrs) {
        printf("attr %s = %d\n", ent.first.c_str(), ent.second);
    }
    for (auto &ent : prog->uniforms) {
        printf("uniform %s = %d\n", ent.first.c_str(), ent.second);
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
        printf("buffer %s = %u (%zu bytes)\n", name, *obj, v.size() * sizeof(T));
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

static void image_create_texture(GLuint *tex, GLsizei width, GLsizei height,
    int depth, uint8_t *pixels, GLenum filter)
{
    static const GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};

    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    switch (depth) {
    case 1:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, (GLsizei)width, (GLsizei)height,
            0, GL_RED, GL_UNSIGNED_BYTE, (GLvoid*)pixels);
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        break;
    case 4:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)pixels);
        break;
    }
    glActiveTexture(GL_TEXTURE0);
}