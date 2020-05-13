#include "AtmosphericScattering.h"

AtmosphericScattering::AtmosphericScattering()
{
	auto const TRANSMITTANCE_FORMAT = DXGI_FORMAT_R32G32B32A32_FLOAT;


	auto renderer = Renderer::getSingleton();
	auto ps = renderer->compileShaderFromFile("shaders/atmospheric_scattering.hlsl","precomputeTransmittanceToAtmosphereTop",SM_PS);
	mQuad = Quad::Ptr(new Quad);
	mQuad->init(ps, TRANSMITTANCE_FORMAT);
	mAtmosphereConsts = mQuad->getPipelineState()->createConstantBuffer(Renderer::Shader::ST_PIXEL, "AtomsphereConstants");

	auto size = renderer->getSize();
	mTransmittanceToAtmosphereTop = renderer->createResourceView(256, 256, TRANSMITTANCE_FORMAT,Renderer::VT_RENDERTARGET);
	mTransmittanceToAtmosphereTop->createRenderTargetView(NULL);
	mTransmittanceToAtmosphereTop->createShaderResource(NULL);

	mSettings = ImGuiOverlay::ImGuiObject::root()->createChild<ImGuiOverlay::ImGuiWindow>("atmosphere settings");
	mSettings->drawCallback = [&, & settings = mAtmosphereSettings](auto ui) {
		
		return true;
	};

	initAtmospehre();
}

void AtmosphericScattering::execute(RenderGraph& graph)
{
	if (mRecompute)
	{	
		mRecompute  = false;
		graph.addPass("prec", [this](RenderGraph::Builder& b)mutable
		{
			return [this](Renderer::CommandList::Ref cmdlist) {
				cmdlist->setRenderTarget(mTransmittanceToAtmosphereTop);
				mQuad->setRect({0,0,256,256});
				mQuad->setConstants("AtomsphereConstants",mAtmosphereConsts);
				mQuad->draw(cmdlist);

				//cmdlist->transitionBarrier(mTransmittanceToAtmosphereTop, D3D12_RESOURCE_STATE_RENDER_TARGET, 0, true);
			};
		});
	}
	else
	{
	}
}

Renderer::Resource::Ref AtmosphericScattering::getTransmittanceToTop()
{
	return mTransmittanceToAtmosphereTop;
}

void AtmosphericScattering::initAtmospehre()
{

	constexpr double kLambdaR = 680.0;
	constexpr double kLambdaG = 550.0;
	constexpr double kLambdaB = 440.0;

	constexpr double mie_g_coefficient = 0.8;
	constexpr bool use_half_precision_ = true;
	constexpr bool use_constant_solar_spectrum_ = false;
	constexpr bool use_ozone_ = true;
	constexpr bool use_rayleigh_scattering = true;
	constexpr bool use_mie_scattering = true;

	constexpr double kPi = 3.1415926;
	constexpr double kSunAngularRadius = 0.00935 / 2.0;
	constexpr double kSunSolidAngle = kPi * kSunAngularRadius * kSunAngularRadius;
	constexpr double kLengthUnitInMeters = 1000.0;

	constexpr int kLambdaMin = 360;
	constexpr int kLambdaMax = 830;
	constexpr double kSolarIrradiance[48] = {
		1.11776, 1.14259, 1.01249, 1.14716, 1.72765, 1.73054, 1.68870, 1.61253,
		1.91198, 2.03474, 2.02042, 2.02212, 1.93377, 1.95809, 1.91686, 1.82980,
		1.86850, 1.89310, 1.85149, 1.85040, 1.83410, 1.83450, 1.81470, 1.78158, 1.7533,
		1.69650, 1.68194, 1.64654, 1.60480, 1.52143, 1.55622, 1.51130, 1.47400, 1.4482,
		1.41018, 1.36775, 1.34188, 1.31429, 1.28303, 1.26758, 1.23670, 1.20820,
		1.18737, 1.14683, 1.12362, 1.10580, 1.07124, 1.04992
	};

	// http://www.iup.uni-bremen.de/gruppen/molspec/databases
	// /referencespectra/o3spectra2011/index.html
	constexpr double kOzoneCrossSection[48] = {
		1.18e-27, 2.182e-28, 2.818e-28, 6.636e-28, 1.527e-27, 2.763e-27, 5.52e-27,
		8.451e-27, 1.582e-26, 2.316e-26, 3.669e-26, 4.924e-26, 7.752e-26, 9.016e-26,
		1.48e-25, 1.602e-25, 2.139e-25, 2.755e-25, 3.091e-25, 3.5e-25, 4.266e-25,
		4.672e-25, 4.398e-25, 4.701e-25, 5.019e-25, 4.305e-25, 3.74e-25, 3.215e-25,
		2.662e-25, 2.238e-25, 1.852e-25, 1.473e-25, 1.209e-25, 9.423e-26, 7.455e-26,
		6.566e-26, 5.105e-26, 4.15e-26, 4.228e-26, 3.237e-26, 2.451e-26, 2.801e-26,
		2.534e-26, 1.624e-26, 1.465e-26, 2.078e-26, 1.383e-26, 7.105e-27
	};
	// https://en.wikipedia.org/wiki/Dobson_unit, in molecules.m^-2.
	constexpr double kDobsonUnit = 2.687e20;
	constexpr double kMaxOzoneNumberDensity = 300.0 * kDobsonUnit / 15000.0;
	constexpr double kConstantSolarIrradiance = 1.5;
	constexpr double kBottomRadius = 6360000.0;
	constexpr double kTopRadius = 6420000.0;
	constexpr double kRayleigh = 1.24062e-6;
	constexpr double kRayleighScaleHeight = 8000.0;
	constexpr double kMieScaleHeight = 1200.0;
	constexpr double kMieAngstromAlpha = 0.0;
	constexpr double kMieAngstromBeta = 5.328e-3;
	constexpr double kMieSingleScatteringAlbedo = 0.9;
	double kMiePhaseFunctionG = mie_g_coefficient;
	constexpr double kGroundAlbedo = 0.1;
	const double max_sun_zenith_angle =
		(use_half_precision_ ? 102.0 : 120.0) / 180.0 * kPi;

	// rayleigh层，即空气分子层     //width,exp_term,exp_scale,linear_term,constant_term
	DensityProfileLayer rayleigh_layer = {0.0, 1.0 * use_rayleigh_scattering,
		-1.0 / kRayleighScaleHeight * use_rayleigh_scattering,
		0.0, 0.0};
	// mie层，气溶胶层            //width,exp_term,exp_scale,linear_term,constant_term
	DensityProfileLayer mie_layer = {0.0, 1.0 * use_mie_scattering,
		-1.0 / kMieScaleHeight * use_mie_scattering, 0.0, 0.0};
	std::vector<DensityProfileLayer> ozone_density;
	ozone_density.push_back( {25000.0, 0.0, 0.0, 1.0 / 15000.0, -2.0 / 3.0});
	ozone_density.push_back({0.0, 0.0, 0.0, -1.0 / 15000.0, 8.0 / 3.0});



	std::vector<double> wavelengths;            // 波长
	std::vector<double> solar_irradiance;       // 太阳辐照度
	std::vector<double> rayleigh_scattering;    // rayleigh散射
	std::vector<double> mie_scattering;         // mie散射
	std::vector<double> mie_extinction;         // mie消光
	std::vector<double> absorption_extinction;  // 吸收光线的空气分子消光
	std::vector<double> ground_albedo;          // 地面反照率
	for (int l = kLambdaMin; l <= kLambdaMax; l += 10) {
		double lambda = static_cast<double>(l) * 1e-3;
		double mie =
			kMieAngstromBeta / kMieScaleHeight * pow(lambda, -kMieAngstromAlpha);
		// 太阳光波波长
		wavelengths.push_back(l);
		// 太阳辐照度
		if (use_constant_solar_spectrum_) {//常量
			solar_irradiance.push_back(kConstantSolarIrradiance);
		}
		else {
			solar_irradiance.push_back(kSolarIrradiance[(l - kLambdaMin) / 10]);
		}
		rayleigh_scattering.push_back(kRayleigh * pow(lambda, -4));
		mie_scattering.push_back(mie * kMieSingleScatteringAlbedo);
		mie_extinction.push_back(mie);
		absorption_extinction.push_back(
			use_ozone_ * kMaxOzoneNumberDensity * kOzoneCrossSection[(l - kLambdaMin) / 10]);
		ground_albedo.push_back(kGroundAlbedo);
	}

	auto interpolate = [&](auto& function, double wavelength)
	{
		if (wavelength < wavelengths[0]) {
			return function[0];
		}
		for (unsigned int i = 0; i < wavelengths.size() - 1; ++i) {
			if (wavelength < wavelengths[i + 1]) {// 线性插值
				double u =
					(wavelength - wavelengths[i]) / (wavelengths[i + 1] - wavelengths[i]);
				return
					function[i] * (1.0 - u) + function[i + 1] * u;
			}
		}
		return function[function.size() - 1];
	};

	float3 lambdas{ kLambdaR, kLambdaG, kLambdaB };

	auto cal_val = [&](auto& v, double scale) {
		float r = interpolate(v, lambdas[0]) * scale;
		float g = interpolate(v, lambdas[1]) * scale;
		float b = interpolate(v, lambdas[2]) * scale;
		return float3{r,g,b};
	};


	mAtmosphereParams = {
		{
			cal_val(solar_irradiance,  1.0),
			kSunAngularRadius,

			cal_val(rayleigh_scattering,  kLengthUnitInMeters),
			kBottomRadius,
			
			cal_val(mie_scattering,  kLengthUnitInMeters),
			kTopRadius,
			
			cal_val(mie_extinction,  kLengthUnitInMeters),
			(float)kMiePhaseFunctionG,

			cal_val(absorption_extinction,kLengthUnitInMeters),
			max_sun_zenith_angle,

			cal_val(ground_albedo, 1.0),
			0,

			{{{}, rayleigh_layer}},
			{{{}, mie_layer}},
			{{ozone_density[0], ozone_density[1]}},
		},
		{256,256}
	};

	if (mAtmosphereConsts)
		mAtmosphereConsts->blit(&mAtmosphereParams, 0, sizeof(mAtmosphereParams));
}
