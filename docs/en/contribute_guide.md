# Contribution Guide

Welcome to contribute to [TuyaOpen](https://github.com/tuya/TuyaOpen) through the Fork and [Push Requests](https://docs.github.com/zh/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests) process on GitHub, including fixing functional code and documentation errors, adding new features and documentation, etc.

## Changes
1. Install the git tool on your local PC.
2. Fork the TuyaOpen repository to your own GitHub repository.
3. Clone the forked TuyaOpen repository to your local PC using the `git clone` command.
4. Create your local development branch from the branch to be merged using the `git checkout -b <BranchName>` command.
5. Modify the code or documentation content to be submitted locally.
6. Submit the modifications to your GitHub repository.

## Preparation
1. TuyaOpen uses the Apache License 2.0 open source license. Check if the contributed content has obtained an open source license compatible with the Apache License 2.0. If not, we will not accept it.
2. Check if the code follows the [TuyaOpen coding style guide](./code_style_guide.md) and if the code is well-commented for readers to understand its structure.
3. Check if documentation or examples are provided for the contributed code, as good examples and documentation will help other developers use it better.
4. Check if the documentation follows the [documentation writing guidelines]().

5. If submitting multiple contents, check if all contents are grouped according to the type of modification (each Pull Request corresponds to a major change).

6. If contributing content related to chip adaptation under board, it must comply with the [TuyaOpen chip adaptation guidelines]().

After checking, you can proceed to the Push Requests process.

## Submitting Push Requests
Select the branch you have modified in your TuyaOpen repository under your GitHub account, and click the New pull request button to initiate a Pull Request.

When developers submit their first contribution, they need to sign the Contributor License Agreement. Please confirm that the CLA shows a successful sign.

After creating the Pull Request, it will enter the automated CI compilation and testing process, followed by manual review.

During the Pull Request process, there may be some discussions about the request in the PR comments section. Please check the PR status and update the code and documentation according to the review comments.

After testing and review are passed, the changes will be merged into the TuyaOpen repository.

This marks the end of the contribution process.