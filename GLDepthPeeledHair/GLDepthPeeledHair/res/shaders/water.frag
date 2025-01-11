#version 460 core

layout (location = 0) out vec4 FragOut;

layout (binding = 0) uniform sampler2D reflectionTexture;
layout (binding = 1) uniform sampler2D refractionTexture;
layout (binding = 2) uniform sampler2D waterDUDVTexture;
layout (binding = 3) uniform sampler2D waterNormalTexture;

in vec2 TexCoord;
in vec4 ClipSpace;
in vec3 WorldPos;
uniform float viewportWidth;
uniform float viewportHeight;
uniform vec3 viewPos;
uniform float time;


const float tiling = 0.25;
const float waveStrength = 0.01;
const float timeFactorX = 0.0125;
const float timeFactorY = -0.00125;
const float normalMapScale = 0.95;
const float shineDamper = 40.0;
const float reflectivity = 0.25;
const float waterNormalDampening = 0.25;

void main() {
    discard;
    vec2 refractionUV = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);
    vec2 reflectionUV = vec2(refractionUV.x, 1 - refractionUV.y);
    vec2 waterUV = TexCoord * tiling;

    float moveFactorX = time * timeFactorX;
    float moveFactorY = time * timeFactorY;
    
    vec2 distortedTexCoords = texture(waterDUDVTexture, vec2(waterUV.x + moveFactorX, waterUV.y)).rg*0.1;
	distortedTexCoords = waterUV + vec2(distortedTexCoords.x, distortedTexCoords.y + moveFactorY);
	vec2 totalDistortion = (texture(waterDUDVTexture, distortedTexCoords).rg * 2.0 - 1.0) * waveStrength;

   // vec2 distortion1 = (texture(waterDUDVTexture, vec2(waterUV.x + moveFactorX, waterUV.y)).rg * 2.0 - 1.0) * waveStrength;
   // vec2 distortion2 = (texture(waterDUDVTexture, vec2(-waterUV.x + moveFactorX, waterUV.y + moveFactorY)).rg * 2.0 - 1.0) * waveStrength;
   // vec2 totalDistortion = distortion1 = distortion2;

    reflectionUV += totalDistortion;
    refractionUV += totalDistortion;
    
    vec3 reflectionColor = texture2D(reflectionTexture, reflectionUV).rgb;
    vec3 refractionColor = texture2D(refractionTexture, refractionUV).rgb;
    
    vec3 normalMapColor = texture(waterNormalTexture, distortedTexCoords * normalMapScale).rgb;
    vec3 normal = normalize(vec3(normalMapColor.r * 2 - 1, normalMapColor.b, normalMapColor.g * 2 - 1));

    normal = mix(normal, vec3(0, 1, 0), waterNormalDampening);


    vec3 lightPosition = (vec3(7, 0, 10) * 0.5) + vec3(0, 1, 0);
    //vec3 lightColor = vec3(1, 1, 1);
    vec3 lightColor = vec3(1, 0.98, 0.94);
    float lightRadius = 10;
    float lightStrength = 10;

    
    vec3 lightDir = normalize(lightPosition - WorldPos);
    vec3 viewDir = normalize(viewPos - WorldPos);
    vec3 halfVec = normalize(lightDir + viewDir);
    float distance = length(lightPosition - WorldPos);
    float attenuation = 1.0 / (distance * distance);  // Inverse square law for point light attenuation
    float spec = max(dot(halfVec, normal), 0.0);
    spec = pow(spec, shineDamper);
    vec3 specular = lightColor * lightStrength * spec * attenuation * reflectivity;

    const vec3 lightDirection = normalize(lightPosition - WorldPos);

    FragOut.rgb = vec3(0,0,1);

    vec3 viewVector = normalize(viewPos - WorldPos);
    float relfractiveFactor = dot(viewVector, vec3(0, 1, 0));


    
    vec3 waterColor = vec3(0.4, 0.8, 0.6);
    //relfractiveFactor = 0.5;
    
    //vec3 finalWaterColor = mix(refractionColor, reflectionColor, relfractiveFactor);
    vec3 finalWaterColor = mix(reflectionColor, refractionColor, relfractiveFactor);
    //finalWaterColor = mix(finalWaterColor, refractionColor, 0.5);
    finalWaterColor = mix(finalWaterColor, waterColor, 0.2);
    finalWaterColor *= vec3(0.5);

    finalWaterColor += specular;

    FragOut.rgb = finalWaterColor;
	FragOut.a = 1.0;

    //FragOut.rgb = normalMapColor;
    //FragOut.rgb = normal;
    //FragOut.rgb = vec3(spec);

    
   // FragOut.rgb = vec3(refractionUV,0);;

}
