# AAX Signing Approval Request - MasterLimiter v0.3.0

Use this template to contact Avid/PACE and request production AAX signing
approval for MasterLimiter.

## Subject

AAX signing approval request - MasterLimiter v0.3.0 (MelechDSP)

## Email Body

Hello Avid/PACE team,

I am requesting guidance and approval to complete the AAX production distribution
signing flow for our plugin.

Company / Publisher
- Company name: MelechDSP
- Contact name: Avishay Lidani
- Contact email: avishay.lidani@gmail.com
- iLok/PACE account: avishayl
- Avid developer account email: avishay.lidani@gmail.com

Product
- Product name: MasterLimiter
- Plugin format: AAX
- Release candidate version: 0.3.0
- Manufacturer code: Melc
- Plugin code: MaLm
- AAX bundle identifier: com.MelechDSP.MasterLimiter
- CFBundleVersion: 0.3.0
- CFBundleShortVersionString: 0.3.0
- Target platforms:
  - macOS 10.13+ (universal: x86_64 + arm64 when built universal)

Current status
- AAX builds with `AAX_SDK_PATH` configured.
- Retail Pro Tools validation is pending MasterLimiter-specific PACE signing.
- Unsigned AAX validation, if any, is limited to Avid developer builds of Pro Tools.

Request
Please provide the required production signing workflow details for our account,
including:

1. Account enablement/approval prerequisites for MasterLimiter AAX production signing.
2. Required tooling/workflow to sign `MasterLimiter.aaxplugin` for commercial distribution.
3. MasterLimiter-specific wrap config GUID or customer-number/product setup.
4. Required metadata/submission package details.
5. Validation requirements before final release sign-off.

If helpful, I can provide:
- Release candidate bundle: `MasterLimiter.aaxplugin`
- Release metadata and identifiers listed above
- SHA256 of the release-candidate AAX binary: `[fill in per build]`
- QA report: `[attach when available]`

Thank you,
Avishay Lidani
MelechDSP

## Notes

- MasterLimiter needs its own Avid/PACE product and wrap config. AnalyzerPro's
  `WCGUID`, product, and approval cannot be reused.
- Apple Developer ID signing/notarization is separate from PACE/AAX signing.
- Final public/commercial AAX release remains pending PACE/Avid production
  signing completion.
