export async function enterXR(event, status, macro) {
  if (!navigator.xr)
    throw Error(
      "This browser does not provide immersive VR. The flat keyboard is available.",
    );
  if (navigator.xr.isSessionSupported && !(await navigator.xr.isSessionSupported("immersive-vr")))
    throw Error("Immersive VR unavailable. The flat keyboard is available.");
  const session = await navigator.xr.requestSession("immersive-vr");
  const canvas = document.createElement("canvas"),
    gl = canvas.getContext("webgl", { xrCompatible: true });
  const notes = new Map(),
    grips = new Map(),
    positions = [
      [-0.45, 0, -1.2],
      [0, 0, -1.2],
      [0.45, 0, -1.2],
    ];
  const nearest = (p) =>
    positions.reduce(
      (best, v, i) =>
        Math.hypot(v[0] - p.x, v[1] - p.y, v[2] - p.z) <
        Math.hypot(
          positions[best][0] - p.x,
          positions[best][1] - p.y,
          positions[best][2] - p.z,
        )
          ? i
          : best,
      0,
    );
  function release(source) {
    if (notes.has(source)) {
      event(1, notes.get(source), 0);
      notes.delete(source);
    }
    grips.delete(source);
  }
  session.addEventListener("end", () => {
    for (const s of notes.keys()) release(s);
    positions.forEach((_, i) => {
      event(8, i, 0);
      event(10, i, 1);
    });
    status("Returned to the flat instrument.");
  });
  try {
    if (!gl) throw Error("WebGL unavailable");
    await gl.makeXRCompatible();
    session.updateRenderState({ baseLayer: new XRWebGLLayer(session, gl) });
    const space = await session.requestReferenceSpace("local");
    const program = gl.createProgram();
    function shader(type, text) {
      const s = gl.createShader(type);
      gl.shaderSource(s, text);
      gl.compileShader(s);
      if (!gl.getShaderParameter(s, gl.COMPILE_STATUS))
        throw Error(gl.getShaderInfoLog(s));
      gl.attachShader(program, s);
    }
    shader(
      gl.VERTEX_SHADER,
      "attribute vec3 p;uniform mat4 proj,view;void main(){gl_Position=proj*view*vec4(p,1.);gl_PointSize=54./max(.5,gl_Position.w);}",
    );
    shader(
      gl.FRAGMENT_SHADER,
      "precision mediump float;uniform vec3 color;void main(){if(distance(gl_PointCoord,vec2(.5))>.5)discard;gl_FragColor=vec4(color,1.);}",
    );
    gl.linkProgram(program);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS))
      throw Error(gl.getProgramInfoLog(program));
    gl.useProgram(program);
    const loc = (k) => gl.getUniformLocation(program, k);
    const point = gl.getAttribLocation(program, "p"),
      proj = loc("proj"),
      view = loc("view"),
      color = loc("color");
    const get = (e) =>
      e.frame.getPose(
        e.inputSource.gripSpace || e.inputSource.targetRaySpace,
        space,
      )?.transform.position;
    session.addEventListener("selectstart", (e) => {
      const p = get(e);
      if (!p) return;
      const n =
        60 + nearest(p) * 4 + (e.inputSource.handedness === "right" ? 12 : 0);
      notes.set(e.inputSource, n);
      event(0, n, 0.85, e.timeStamp);
    });
    session.addEventListener("selectend", (e) => release(e.inputSource));
    session.addEventListener("squeezestart", (e) => {
      const p = get(e);
      if (p) grips.set(e.inputSource, nearest(p));
    });
    session.addEventListener("squeezeend", (e) => grips.delete(e.inputSource));
    session.addEventListener("inputsourceschange", (e) =>
      e.removed.forEach(release),
    );
    function frame(time, frame) {
      session.requestAnimationFrame(frameLoop);
      const pose = frame.getViewerPose(space);
      if (!pose) return;
      for (const [s, i] of grips) {
        const p = frame.getPose(s.gripSpace || s.targetRaySpace, space)
          ?.transform.position;
        if (p) {
          positions[i] = [p.x, p.y, p.z];
          macro(0, Math.max(0, Math.min(1, p.y + 0.5)));
        }
      }
      const t = pose.transform.inverse.matrix;
      positions.forEach(([x, y, z], i) => {
        const lateral = t[0] * x + t[4] * y + t[8] * z + t[12],
          depth = t[2] * x + t[6] * y + t[10] * z + t[14];
        event(10, i, Math.hypot(lateral, depth), time);
        event(8, i, lateral / Math.max(0.1, Math.hypot(lateral, depth)), time);
      });
      const layer = session.renderState.baseLayer;
      gl.bindFramebuffer(gl.FRAMEBUFFER, layer.framebuffer);
      gl.clearColor(0.025, 0.06, 0.09, 1);
      gl.clear(gl.COLOR_BUFFER_BIT);
      for (const eye of pose.views) {
        const vp = layer.getViewport(eye);
        gl.viewport(vp.x, vp.y, vp.width, vp.height);
        gl.uniformMatrix4fv(proj, false, eye.projectionMatrix);
        gl.uniformMatrix4fv(view, false, eye.transform.inverse.matrix);
        positions.forEach((p, i) => {
          gl.vertexAttrib3fv(point, p);
          gl.uniform3f(color, 0.2 + i * 0.22, 0.86 - i * 0.12, 0.88);
          gl.drawArrays(gl.POINTS, 0, 1);
        });
      }
    }
    const frameLoop = frame;
    session.requestAnimationFrame(frameLoop);
    status("Trigger plays a note. Grip moves a layer.");
  } catch (e) {
    await session.end();
    throw e;
  }
}
