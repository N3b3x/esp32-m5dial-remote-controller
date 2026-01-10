---
layout: default
title: "Contributing"
description: "How to contribute to the HardFOC ESP-IDF CI Tools project"
nav_order: 4
parent: "📚 Reference"
permalink: /docs/contributing/
---

# Contributing to HardFOC ESP-IDF CI Tools

We welcome contributions to the HardFOC ESP-IDF CI Tools project! This guide explains how you can help improve the project.

## 🤝 How to Contribute

### Types of Contributions

- **🐛 Bug Reports** - Report issues and bugs
- **✨ Feature Requests** - Suggest new features
- **📝 Documentation** - Improve documentation
- **🔧 Code Contributions** - Submit code improvements
- **🧪 Testing** - Help test new features
- **💬 Community Support** - Help other users

### Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally
3. **Create a feature branch** for your changes
4. **Make your changes** and test them
5. **Submit a pull request** with a clear description

## 🐛 Reporting Issues

### Before Reporting

- **Check existing issues** to avoid duplicates
- **Search discussions** for similar problems
- **Test with latest version** to ensure it's not fixed

### Issue Template

When reporting issues, include:

```markdown
## Bug Description
Brief description of the issue

## Steps to Reproduce
1. Step one
2. Step two
3. Step three

## Expected Behavior
What should happen

## Actual Behavior
What actually happens

## Environment
- OS: [e.g., Ubuntu 20.04]
- ESP-IDF Version: [e.g., v5.5]
- Project Type: [e.g., Basic ESP32 project]

## Additional Context
Any other relevant information
```

## ✨ Feature Requests

### Before Requesting

- **Check existing features** to avoid duplicates
- **Search discussions** for similar requests
- **Consider the project scope** and goals

### Feature Request Template

```markdown
## Feature Description
Brief description of the requested feature

## Use Case
Why is this feature needed?

## Proposed Solution
How should this feature work?

## Alternatives Considered
What other approaches were considered?

## Additional Context
Any other relevant information
```

## 🔧 Code Contributions

### Development Setup

1. **Fork and clone** the repository
2. **Install dependencies**:
   ```bash
   # Install Jekyll for documentation
   gem install jekyll bundler
   
   # Install Python dependencies
   pip install -r requirements.txt
   ```

3. **Set up development environment**:
   ```bash
   # Install ESP-IDF
   ./setup_repo.sh
   
   # Test configuration
   ./scripts/config_loader.sh
   ```

### Code Style

- **Follow existing patterns** in the codebase
- **Use meaningful variable names** and comments
- **Write tests** for new functionality
- **Update documentation** as needed

### Testing

Before submitting:

- **Test locally** with your changes
- **Run existing tests** to ensure no regressions
- **Test with different ESP-IDF versions** if applicable
- **Verify documentation** builds correctly

### Pull Request Process

1. **Create feature branch** from main
2. **Make focused changes** (one feature per PR)
3. **Write clear commit messages**
4. **Update documentation** if needed
5. **Test thoroughly** before submitting
6. **Submit PR** with detailed description

## 📝 Documentation Contributions

### Types of Documentation

- **User guides** and tutorials
- **API documentation** and references
- **Configuration examples** and templates
- **Troubleshooting guides** and FAQs

### Documentation Guidelines

- **Use clear, concise language**
- **Include code examples** where helpful
- **Follow the existing style** and structure
- **Test all examples** before submitting
- **Update navigation** if adding new pages

### Building Documentation

```bash
# Install Jekyll
gem install jekyll bundler

# Build documentation
cd docs
bundle install
bundle exec jekyll serve

# View at http://localhost:4000
```

## 🧪 Testing Contributions

### Types of Testing

- **Unit tests** for individual functions
- **Integration tests** for workflows
- **End-to-end tests** for complete pipelines
- **Performance tests** for optimization

### Test Guidelines

- **Write tests** for new functionality
- **Update tests** when fixing bugs
- **Test edge cases** and error conditions
- **Document test requirements** and setup

## 💬 Community Support

### Ways to Help

- **Answer questions** in discussions
- **Help troubleshoot** user issues
- **Share examples** and use cases
- **Improve documentation** based on user feedback

### Community Guidelines

- **Be respectful** and inclusive
- **Provide helpful** and constructive feedback
- **Search before asking** to avoid duplicates
- **Use appropriate channels** for different types of discussions

## 🏗️ Project Structure

Understanding the project structure:

```
hf-espidf-project-tools/
├── .github/workflows/     # GitHub Actions workflows
├── docs/                  # Documentation (Jekyll)
├── *.sh                   # Utility Scripts
├── *.py                   # Python Scripts
└── README.md              # Project overview
```

### Key Components

- **Workflows** - GitHub Actions CI/CD pipelines
- **Scripts** - Build, flash, and utility scripts
- **Configuration** - YAML configuration system
- **Documentation** - Jekyll-based documentation site

## 📋 Contribution Checklist

Before submitting:

- [ ] **Code follows** project style guidelines
- [ ] **Tests pass** and new tests are added
- [ ] **Documentation updated** for new features
- [ ] **Commit messages** are clear and descriptive
- [ ] **Pull request** has detailed description
- [ ] **No breaking changes** without discussion

## 🎯 Development Priorities

Current focus areas:

1. **Performance optimization** - Faster builds and CI
2. **Security enhancements** - Better security scanning
3. **Documentation improvements** - More examples and guides
4. **Multi-platform support** - Windows and macOS improvements
5. **Advanced features** - More configuration options

## 📞 Getting Help

### Communication Channels

- **[GitHub Issues](https://github.com/n3b3x/hf-espidf-project-tools/issues)** - Bug reports and feature requests
- **[GitHub Discussions](https://github.com/n3b3x/hf-espidf-project-tools/discussions)** - Questions and community support
- **[Pull Requests](https://github.com/n3b3x/hf-espidf-project-tools/pulls)** - Code contributions

### Maintainer Contact

- **Primary Maintainer**: N3b3x
- **GitHub**: [@N3b3x](https://github.com/N3b3x)
- **Email**: Available through GitHub profile

## 📜 Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inclusive experience for everyone, regardless of:

- Age, body size, disability, ethnicity
- Gender identity and expression
- Level of experience, education
- Nationality, personal appearance
- Race, religion, sexual orientation

### Expected Behavior

- **Use welcoming and inclusive language**
- **Be respectful** of differing viewpoints
- **Accept constructive criticism** gracefully
- **Focus on what's best** for the community
- **Show empathy** towards other community members

### Unacceptable Behavior

- **Harassment** or discrimination
- **Trolling** or inflammatory comments
- **Personal attacks** or political discussions
- **Spam** or off-topic discussions
- **Other unprofessional conduct**

## 📄 License

By contributing, you agree that your contributions will be licensed under the same license as the project (MIT License).

## 🙏 Recognition

Contributors will be recognized in:

- **README.md** - Contributor list
- **Release notes** - Feature contributors
- **Documentation** - Example contributors
- **GitHub** - Contributor profiles

---

**Ready to contribute?** Start by [forking the repository](https://github.com/n3b3x/hf-espidf-project-tools/fork) and checking out our [GitHub Issues](https://github.com/n3b3x/hf-espidf-project-tools/issues) for good first issues to work on!
