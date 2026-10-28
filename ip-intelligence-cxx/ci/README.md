# API Specific CI Approach

For the general CI Approach, see [common-ci](https://github.com/51degrees/common-ci).

The following secrets are required:
* `ACCESS_TOKEN` - GitHub [access token](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens#about-personal-access-tokens) for cloning repos, creating PRs, etc.
    * Example: `github_pat_l0ng_r4nd0m_s7r1ng`
  
The following secrets are required to run tests:
* `IPI_DATA_FILE_URL` - [URL](https://51degrees.com/pricing?utm_source=github&utm_medium=readme&utm_campaign=ip-intelligence-cxx&utm_content=ci-readme.md&utm_term=api-specific-ci-approach) for downloading the enterprise IP Intelligence data file (`51Degrees-EnterpriseIpiV41.ipi`)
    * The workflows pass it to `ci/fetch-assets.ps1` as the `-IpIntelligenceUrl` parameter.

### Differences
- There are no packages produced by this repository, so the only output from the `Nightly Publish Main` workflow is a new tag and release.
- The package update step does not update dependencies from a package manager in the same way as other repos.

### Build Options

For additional build options in this repo see [common-ci/cxx](https://github.com/51Degrees/common-ci/tree/main/cxx#readme)

## Prerequisites

In addition to the [common prerequisites](https://github.com/51Degrees/common-ci#prerequisites), the following is required to fetch the assets used by the tests:
- `IPI_DATA_FILE_URL` - Url to download the IP Intelligence data file from
