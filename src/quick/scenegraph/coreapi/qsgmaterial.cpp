/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgmaterial.h"
#include "qsgrenderer_p.h"

QT_BEGIN_NAMESPACE


/*!
    \class QSGMaterialShader
    \brief The QSGMaterialShader class implements a material renders geometry.

    The QSGMaterial and QSGMaterialShader form a tight relationship. For one
    scene graph (including nested graphs), there is one unique QSGMaterialShader
    instance which encapsulates the QOpenGLShaderProgram the scene graph uses
    to render that material, such as a shader to flat coloring of geometry.
    Each QSGGeometryNode can have a unique QSGMaterial containing the
    how the shader should be configured when drawing that node, such as
    the actual color to used to render the geometry.

    An instance of QSGMaterialShader is never created explicitely by the user,
    it will be created on demand by the scene graph through
    QSGMaterial::createShader(). The scene graph will make sure that there
    is only one instance of each shader implementation through a scene graph.

    The source code returned from vertexShader() is used to control what the
    material does with the vertiex data that comes in from the geometry.
    The source code returned from the fragmentShader() is used to control
    what how the material should fill each individual pixel in the geometry.
    The vertex and fragment source code is queried once during initialization,
    changing what is returned from these functions later will not have
    any effect.

    The activate() function is called by the scene graph when a shader is
    is starting to be used. The deactivate function is called by the scene
    graph when the shader is no longer going to be used. While active,
    the scene graph may make one or more calls to updateState() which
    will update the state of the shader for each individual geometry to
    render.

    The attributeNames() returns the name of the attributes used in the
    vertexShader(). These are used in the default implementation of
    activate() and deactive() to decide whice vertex registers are enabled.

    The initialize() function is called during program creation to allow
    subclasses to prepare for use, such as resolve uniform names in the
    vertexShader() and fragmentShader().

    A minimal example:
    \code
        class Shader : public QSGMaterialShader
        {
        public:
            const char *vertexShader() const {
                return
                "attribute highp vec4 vertex;          \n"
                "uniform highp mat4 matrix;            \n"
                "void main() {                         \n"
                "    gl_Position = matrix * vertex;    \n"
                "}";
            }

            const char *fragmentShader() const {
                return
                "uniform lowp float opacity;                            \n"
                "void main() {                                          \n"
                        "    gl_FragColor = vec4(1, 0, 0, 1) * opacity; \n"
                "}";
            }

            char const *const *attributeNames() const
            {
                static char const *const names[] = { "vertex", 0 };
                return names;
            }

            void initialize()
            {
                QSGMaterialShader::initialize();
                m_id_matrix = program()->uniformLocation("matrix");
                m_id_opacity = program()->uniformLocation("opacity");
            }

            void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
            {
                Q_ASSERT(program()->isLinked());
                if (state.isMatrixDirty())
                    program()->setUniformValue(m_id_matrix, state.combinedMatrix());
                if (state.isOpacityDirty())
                    program()->setUniformValue(m_id_opacity, state.opacity());
            }

        private:
            int m_id_matrix;
            int m_id_opacity;
        };
    \endcode

    \warning Instances of QSGMaterialShader belongs to the Scene Graph rendering
    thread, and cannot be used from the GUI thread.

 */



/*!
    Creates a new QSGMaterialShader.
 */
QSGMaterialShader::QSGMaterialShader()
{
}



/*!
    \fn QOpenGLShaderProgram *QSGMaterialShader::program() const

    Returns the shader program used by this QSGMaterialShader.
 */



/*!
    This function is called by the scene graph to indicate that geometry is
    about to be rendered using this shader.

    State that is global for all uses of the shader, independent of the geometry
    that is being drawn, can be setup in this function.

    If reimplemented, make sure to either call the base class implementation to
    enable the vertex attribute registers.
 */

void QSGMaterialShader::activate()
{
    Q_ASSERT(program()->isLinked());

    program()->bind();
    char const *const *attr = attributeNames();
    for (int i = 0; attr[i]; ++i) {
        if (*attr[i])
            program()->enableAttributeArray(i);
    }
}



/*!
    This function is called by the scene graph to indicate that geometry will
    no longer to be rendered using this shader.

    If reimplemented, make sure to either call the base class implementation to
    disable the vertex attribute registers.
 */

void QSGMaterialShader::deactivate()
{
    char const *const *attr = attributeNames();
    for (int i = 0; attr[i]; ++i) {
        if (*attr[i])
            program()->disableAttributeArray(i);
    }
}



/*!
    This function is called by the scene graph before geometry is rendered
    to make sure the shader is in the right state.

    The current rendering \a state is passed from the scene graph. If the state
    indicates that any state is dirty, the updateState implementation must
    update accordingly for the geometry to render correctly.

    The subclass specific state, such as the color of a flat color material, should
    be extracted from \a newMaterial to update the color uniforms accordingly.

    The \a oldMaterial can be used to minimze state changes when updating
    material states. The \a oldMaterial is 0 if this shader was just activated.

    \sa activate(), deactivate()
 */

void QSGMaterialShader::updateState(const RenderState & /* state */, QSGMaterial * /* newMaterial */, QSGMaterial * /* oldMaterial */)
{
}



/*!
    This function is called when the shader is initialized to compile the
    actual QOpenGLShaderProgram. Do not call it explicitely.

    The default implementation will extract the vertexShader() and
    fragmentShader() and bind the names returned from attributeNames()
    to consecutive vertex attribute registers starting at 0.
 */

void QSGMaterialShader::compile()
{
    Q_ASSERT_X(!m_program.isLinked(), "QSGSMaterialShader::compile()", "Compile called multiple times!");

    program()->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader());
    program()->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader());

    char const *const *attr = attributeNames();
#ifndef QT_NO_DEBUG
    int maxVertexAttribs = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
    for (int i = 0; attr[i]; ++i) {
        if (i >= maxVertexAttribs) {
            qFatal("List of attribute names is either too long or not null-terminated.\n"
                   "Maximum number of attributes on this hardware is %i.\n"
                   "Vertex shader:\n%s\n"
                   "Fragment shader:\n%s\n",
                   maxVertexAttribs, vertexShader(), fragmentShader());
        }
        if (*attr[i])
            program()->bindAttributeLocation(attr[i], i);
    }
#else
    for (int i = 0; attr[i]; ++i) {
        if (*attr[i])
            program()->bindAttributeLocation(attr[i], i);
    }
#endif

    if (!program()->link()) {
        qWarning("QSGMaterialShader: Shader compilation failed:");
        qWarning() << program()->log();
    }
}



/*!
    \class QSGMaterialShader::RenderState
    \brief The QSGMaterialShader::RenderState encapsulates the current rendering state
    during a call to QSGMaterialShader::updateState().

    The render state contains a number of accessors that the shader needs to respect
    in order to conform to the current state of the scene graph.

    The instance is only valid inside a call to QSGMaterialShader::updateState() and
    should not be used outisde this function.
 */



/*!
    \enum QSGMaterialShader::RenderState::DirtyState

    \value DirtyMatrix Used to indicate that the matrix has changed and must be updated.

    \value DirtyOpacity Used to indicate that the opacity has changed and must be updated.
 */



/*!
    \fn bool QSGMaterialShader::RenderState::isMatrixDirty() const

    Convenience function to check if the dirtyStates() indicates that the matrix
    needs to be updated.
 */



/*!
    \fn bool QSGMaterialShader::RenderState::isOpacityDirty() const

    Conveience function to check if the dirtyStates() indicates that the opacity
    needs to be updated.
 */



/*!
    \fn QSGMaterialShader::RenderState::DirtyStates QSGMaterialShader::RenderState::dirtyStates() const

    Returns which rendering states that have changed and needs to be updated
    for geometry rendered with this material to conform to the current
    rendering state.
 */



/*!
    Returns the accumulated opacity to be used for rendering
 */

float QSGMaterialShader::RenderState::opacity() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->currentOpacity();
}

/*!
    Returns the modelview determinant to be used for rendering
 */

float QSGMaterialShader::RenderState::determinant() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->determinant();
}

/*!
    Returns the matrix combined of modelview matrix and project matrix.
 */

QMatrix4x4 QSGMaterialShader::RenderState::combinedMatrix() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->currentCombinedMatrix();
}



/*!
    Returns the model view matrix.

    If the material has the RequiresFullMatrix flag
    set, this is guaranteed to be the complete transform
    matrix calculated from the scenegraph.

    However, if this flag is not set, the renderer may
    choose to alter this matrix. For example, it may
    pre-transform vertices on the CPU and set this matrix
    to identity.

    In a situation such as the above, it is still possible
    to retrieve the actual matrix determinant by setting
    the RequiresDeterminant flag in the material and
    calling the determinant() accessor.
 */

QMatrix4x4 QSGMaterialShader::RenderState::modelViewMatrix() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->currentModelViewMatrix();
}



/*!
    Returns the viewport rect of the surface being rendered to.
 */

QRect QSGMaterialShader::RenderState::viewportRect() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->viewportRect();
}



/*!
    Returns the device rect of the surface being rendered to
 */

QRect QSGMaterialShader::RenderState::deviceRect() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->deviceRect();
}



/*!
    Returns the QOpenGLContext that is being used for rendering
 */

QOpenGLContext *QSGMaterialShader::RenderState::context() const
{
    return static_cast<const QSGRenderer *>(m_data)->glContext();
}


#ifndef QT_NO_DEBUG
static int qt_material_count = 0;

static void qt_print_material_count()
{
    qDebug("Number of leaked materials: %i", qt_material_count);
    qt_material_count = -1;
}
#endif

/*!
    \class QSGMaterialType
    \brief The QSGMaterialType class is used as a unique type token in combination with QSGMaterial.

    It serves no purpose outside the QSGMaterial::type() function.
 */

/*!
    \class QSGMaterial
    \brief The QSGMaterial class encapsulates rendering state for a shader program.

    The QSGMaterial and QSGMaterialShader subclasses form a tight relationship. For
    one scene graph (including nested graphs), there is one unique QSGMaterialShader
    instance which encapsulates the QOpenGLShaderProgram the scene graph uses
    to render that material, such as a shader to flat coloring of geometry.
    Each QSGGeometryNode can have a unique QSGMaterial containing the
    how the shader should be configured when drawing that node, such as
    the actual color to used to render the geometry.

    The QSGMaterial has two virtual functions that both need to be implemented.
    The function type() should return a unique instance for all instances of a
    specific subclass. The createShader() function should return a new instance
    of QSGMaterialShader, specific to the subclass of QSGMaterial.

    A minimal QSGMaterial implementation could look like this:
    \code
        class Material : public QSGMaterial
        {
        public:
            QSGMaterialType *type() const { static QSGMaterialType type; return &type; }
            QSGMaterialShader *createShader() const { return new Shader; }
        };
    \endcode

    \warning Instances of QSGMaterial belongs to the Scene Graph rendering thread,
    and cannot be used from the GUI thread.
 */

QSGMaterial::QSGMaterial()
    : m_flags(0)
{
#ifndef QT_NO_DEBUG
    ++qt_material_count;
    static bool atexit_registered = false;
    if (!atexit_registered) {
        atexit(qt_print_material_count);
        atexit_registered = true;
    }
#endif
}

QSGMaterial::~QSGMaterial()
{
#ifndef QT_NO_DEBUG
    --qt_material_count;
    if (qt_material_count < 0)
        qDebug("Material destroyed after qt_print_material_count() was called.");
#endif
}



/*!
    \enum QSGMaterial::Flag

    \value Blending Set this flag to true if the material requires GL_BLEND to be
    enabled during rendering.

    \value RequiresDeterminant Set this flag to true if the material relies on
    the determinant of the matrix of the geometry nodes for rendering.

    \value RequiresFullMatrix Set this flag to true if the material relies on
    the full matrix of the geometry nodes for rendering.
 */



/*!
    Sets the flags \a flags on this material if \a on is true;
    otherwise clears the attribute.
*/

void QSGMaterial::setFlag(Flags flags, bool on)
{
    if (on)
        m_flags |= flags;
    else
        m_flags &= ~flags;
}



/*!
    Compares this material to \a other and returns 0 if they are equal; -1 if
    this material should sort before \a other and 1 if \a other should sort
    before.

    The scene graph can reorder geometry nodes to minimize state changes.
    The compare function is called during the sorting process so that
    the materials can be sorted to minimize state changes in each
    call to QSGMaterialShader::updateState().

    The this pointer and \a other is guaranteed to have the same type().
 */

int QSGMaterial::compare(const QSGMaterial *other) const
{
    Q_ASSERT(other && type() == other->type());
    return qint64(this) - qint64(other);
}



/*!
    \fn QSGMaterialType QSGMaterial::type() const

    This function is called by the scene graph to return a unique instance
    per subclass.
 */



/*!
    \fn QSGMaterialShader *QSGMaterial::createShader() const

    This function returns a new instance of a the QSGMaterialShader
    implementatation used to render geometry for a specifc implementation
    of QSGMaterial.

    The function will be called only once for each material type that
    exists in the scene graph and will be cached internally.
*/


QT_END_NAMESPACE
