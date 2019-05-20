#pragma once

typedef unsigned uint;

static std::map<std::string,GLuint> attrs;
static std::map<std::string,GLuint> uniforms;

typedef struct {
    float pos[3];
    float uv[2];
    unsigned color;
} vertex;

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
    length = buf.size();
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
        glGetShaderInfoLog(shader, buf.size() - 1, &length, buf.data());
        printf("shader compile log: %s\n", buf.data());
    }
    
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        printf("failed to compile shader: %s\n", filename);
        exit(1);
    }
    
    return shader;
}

static GLuint link_program(GLuint vsh, GLuint fsh)
{
    GLuint program, n = 1;
    GLint status, numattrs, numuniforms;

    program = glCreateProgram();
    glAttachShader(program, vsh);
    glAttachShader(program, fsh);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        printf("failed to link shader program\n");
        exit(1);
    }

    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &numattrs);
    for (GLint i = 0; i < numattrs; i++)  {
        GLint namelen=-1, size=-1;
        GLenum type = GL_ZERO;
        char namebuf[128];
        glGetActiveAttrib(program, i, sizeof(namebuf)-1, &namelen, &size,
                          &type, namebuf);
        namebuf[namelen] = 0;
        attrs[namebuf] = i;
    }

    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numuniforms);
    for (GLint i = 0; i < numuniforms; i++) {
        GLint namelen=-1, size=-1;
        GLenum type = GL_ZERO;
        char namebuf[128];
        glGetActiveUniform(program, i, sizeof(namebuf)-1, &namelen, &size,
                           &type, namebuf);
        namebuf[namelen] = 0;
        uniforms[namebuf] = glGetUniformLocation(program, namebuf);
    }

    /*
     * Note: OpenGL by default binds attributes to locations counting
     * from zero upwards. This is problematic with at least the Nvidia
     * drvier, where zero has a special meaning. So after linking, we
     * go through the passed attributes and re-assign their bindings
     * starting from 1 counting upwards. We then re-link the program,
     * as we don't know the attribute names until shader is linked.
     */
    for (auto &ent : attrs) {
        glBindAttribLocation(program, (attrs[ent.first] = n++),
            ent.first.c_str());
    }
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        printf("failed to relink shader program\n");
        exit(1);
    }

    glDeleteShader(vsh);
    glDeleteShader(fsh);

    for (auto &ent : attrs) {
        printf("attr %s = %d\n", ent.first.c_str(), ent.second);
    }
    for (auto &ent : uniforms) {
        printf("uniform %s = %d\n", ent.first.c_str(), ent.second);
    }

    return program;
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
static void vertex_array_pointer(const char *attr, GLint size,
    GLenum type, GLboolean norm, X T::*member)
{
    const void *obj = (const void *)reinterpret_cast<std::ptrdiff_t>(
        &(reinterpret_cast<T const *>(NULL)->*member) );
    if (attrs.find(attr) != attrs.end()) {
        glEnableVertexAttribArray(attrs[attr]);
        glVertexAttribPointer(attrs[attr], size, type, norm, sizeof(T), obj);
    }
}

static void vertex_array_1f(const char *attr, float v1)
{
    if (attrs.find(attr) != attrs.end()) {
        glDisableVertexAttribArray(attrs[attr]);
        glVertexAttrib1f(attrs[attr], v1);
    }
}

static void uniform_1i(const char *uniform, GLint i)
{
    if (uniforms.find(uniform) != attrs.end()) {
        glUniform1i(uniforms[uniform], i);
    }
}

static void uniform_matrix_4fv(const char *uniform, const GLfloat *mat)
{
    if (uniforms.find(uniform) != attrs.end()) {
        glUniformMatrix4fv(uniforms[uniform], 1, GL_FALSE, mat);
    }
}
