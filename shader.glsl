#version 330 core
uniform float iTime;
uniform vec2  iResolution;
out vec4 fragColor;

// takes a 2d point and returns a random value 0-1 based on it
float hash(vec2 p) {
    // remove decimal
    return fract(
        // maps to -1 to 1
        sin(
            // dot product smears randomness
            dot(p,
             vec2(127.1, 311.7)
            )
        ) * 43758.5453
    ); // randonmness
}

// smooths random values across a grid
float noise(vec2 p) {
    // snaps to bottom left of grid cell
    vec2 i = floor(p);

    // how far into the cell are we 0-1
    vec2 f = fract(p);
    float a = hash(i), b = hash(i+vec2(1,0));
    float c = hash(i+vec2(0,1)), d = hash(i+vec2(1,1));

    // interpolate vertically across those two results
    return mix(
        mix(a,b,f.x), // mix left and right edge
        mix(c,d,f.x), // mix top and bottom edge
        f.y
    );
}

// take xz and return height of terrain based on noise
float terrain(vec2 p) {
    float h = noise(p * 0.3) * 4.0;
    h += noise(p * 0.9) * 1.4;
    h += noise(p * 2.5) * 0.4;
    h += noise(p * 6.0) * 0.1;
    return h - 2.2;
}

// trinangle density
const float T = 14.0;

const float ALTITUDE = 3.2;

const float FOV = 1.3;

// takes xz and returns normal of the triangle face at that point, and a random id for coloring
vec3 triFlat(
    vec2 xz, 
    out float id // glsl
) {
    // scale xz into grid space where cell is 1x1
    // cell is the one you are in
    // f is where
    vec2 p = xz * T;
    vec2 cell = floor(p);
    vec2 f = fract(p);
    
    // each grid square is in two triangles
    // if f.x + f.y < 1.0, we are in the lower left triangle, otherwise upper right
    bool upper = (f.x + f.y) > 1.0;

    // three corner positions o the triangle you are in in grid space
    // upper triangle has corners at (1,1), (0,1), (1,0)
    // lower triangle has corners at (0,0), (1,0), (0,1)
    vec2 v0 = upper ? cell+vec2(1,1) : cell;
    vec2 v1 = upper ? cell+vec2(0,1) : cell+vec2(1,0);
    vec2 v2 = upper ? cell+vec2(1,0) : cell+vec2(0,1);

    // convert each corner back into T and get the terrain hight at that point to get the 3d position of each corner
    vec3 p0 = vec3(v0.x/T, terrain(v0/T), v0.y/T);
    vec3 p1 = vec3(v1.x/T, terrain(v1/T), v1.y/T);
    vec3 p2 = vec3(v2.x/T, terrain(v2/T), v2.y/T);


    vec3 n = normalize( // makes it length 1
        cross( // cross product of two edges gives normal
            p1-p0,
            p2-p0
        )
    );

    // make all normals point up, if y is negative, flip it
    if (n.y < 0.0) n = -n;

    // id is a unique random float for this triangle
    // v0 is the hash input but offset
    // color variation essentially
    id = hash(v0 + vec2(17.0,31.0) * float(upper));
    return n;
}

// main
void mainImage(
    out vec4 fragColor, // output color
    in vec2 fragCoord // pixel coordinates
) {
    // convert pixel to -1 to 1 with correct aspect ratio
    vec2 uv = (fragCoord - 0.5*iResolution.xy) / iResolution.y;

    // ray origin, moves in X and Z over time but fixed Y
    // change ALTITUDE to make it higher or lower
    vec3 ro = vec3(iTime*1.3, ALTITUDE, iTime*2.0);

    // the direction the camera is pointing
    // 0.5 means slightly right
    // -0.4 means slightly down
    // 1.0 means forward
    // normalizes it
    vec3 forward = normalize(vec3(0.5, -0.4, 1.0));

    // right is perpendicular to forward and up
    vec3 right = normalize(cross(forward, vec3(0,1,0)));

    // up is perpendicular to forward and right
    vec3 up = cross(right, forward);

    // rd is the ray direction
    // get the actual direction pixel points to by combining forward, right, and up with the uv coordinates
    vec3 rd = normalize(uv.x*right + uv.y*up + forward*FOV);

    // sky
    // start with black then add purple
    vec3 col = vec3(0.0);                            
    col = mix(col, vec3(0.10, 0.01, 0.18), exp(-max(rd.y,0.0)*7.0) * 0.6); // exp() makes it exponential so full glow when at the horizon


    // sun
    vec3 sunDir = normalize(vec3(0.6, -0.1, 1.0));
    float sun = pow(max(dot(rd, sunDir), 0.0), 24.0);
    col += vec3(0.4, 0.0, 0.6) * sun;
    
    // finds where the ray hits the terrain
    // t = distance traveled so far
    // p = ro + rd*t = current sampple point along the ray
    // terrain(p.xz) = height pof the terrain at that point
    // if p.y < terrain(p.xz), we are below the terrain and have a hit
    // t+=max((p.y - terrain(p.xz)) * 0.45, 0.015) prevent infinite loop by making sure we always move forward, but move faster when we are far from the terrain and slower when we are close
    // if (t > 55.0) break maximum render distance to prevent infinite loop
    float t = 0.05;
    bool hit = false;
    for (int i = 0; i < 120; i++) {
        vec3 p = ro + rd*t;
        float h = terrain(p.xz);
        if (p.y < h) { hit = true; break; }
        t += max((p.y - h) * 0.45, 0.015);
        if (t > 55.0) break;
    }

    if (hit) {
        // refine the hit
        // find the precise surface crossing
        // p becomes the exact point
        float s = 0.15;
        vec3 p = ro + rd*t;
        for (int i = 0; i < 10; i++) {
            s *= 0.5;
            vec3 m = p - rd*s;
            if (m.y > terrain(m.xz)) p = m;
        }

        // shading the hit point
        // get the normal and id for coloring
        float id;
        vec3 n = triFlat(p.xz, id);

        // how directly does the surface face the light
        // 1.0 means directly facing, 0.0 means perpendicular, negative means facing so we dont get "negative light"
        float diff = clamp(dot(n, normalize(vec3(0.6, 1.0, 0.4))), 0.0, 1.0);

        // colir is a mix of dark and light purple
        // 55% based on the random id for some variation between triangles, and 45% based on the lighting so we can see the shape of the terrain
        vec3 lightPurple = vec3(0.62, 0.22, 0.95);
        vec3 darkPurple  = vec3(0.09, 0.01, 0.16);
        float height = clamp(p.y * 0.4, 0.0, 0.70);
        vec3 peakColor = vec3(0.85, 0.3, 1.0); // near-white light purple for peaks
        col = mix(darkPurple, lightPurple, id * 0.3 + diff * 0.3);
        col = mix(col, peakColor, height * height); // peaks blow out toward bright

        // wireframe edges in XZ grid
        // since this is flat xz, we don't get curved terrains but we get nice sharp triangles
        // f is the xz pos from the current grid cell
        vec2 f = fract(p.xz * T);
        // e is the distance to the nearest triangle edge 
        float e = (f.x+f.y < 1.0)
            ? min(min(f.x, f.y), 1.0-f.x-f.y)
            : min(min(1.0-f.x, 1.0-f.y), f.x+f.y-1.0);
        col = mix(vec3(0.0), col, smoothstep(0.0, 0.05, e)); // 0 right on the edge, and rises to 1.0 at 0.05 distance from the edge. 
                                                                                           // 0.05 is the thickness of the edge 

        // fog
        // t=0 full color
        float distFog   = exp(-t * 0.045);
    float heightFog = exp(-max(p.y + 1.0, 0.0) * 0.8);
    col = mix(vec3(0.04, 0.0, 0.08), col, distFog * (1.0 - heightFog * 0.6));
    }

    // vignette
    // dot(uv,uv) is the distance from the center of the screen, so it is 0 at the center and increases towards the edges
    col *= 1.0 - dot(uv,uv)*0.5;
    fragColor = vec4(col, 1.0);
}


void main() {
    vec2 fc = (gl_FragCoord.xy);
    mainImage(fragColor, fc);
}