# SQLite Cloud Documentation Search

<p align="center">
  <img src="https://sqlitecloud.io/social/logo.png" height="300" alt="SQLite Cloud logo">
</p>

![Build Status](https://github.com/sqlitecloud/docsearch-action/actions/workflows/test.yaml/badge.svg "Build Status")
[![Latest Release](https://img.shields.io/github/v/tag/sqlitecloud/docsearch-action)](https://github.com/sqlitecloud/docsearch-action/releases/latest)

This GitHub Action automates the process of building a table for an out-of-the-box documentation website full-text search powered by SQLite Cloud.

## Features

- Easy setup and configuration
- Seamless integration with your documentation website
- Utilizes the power of SQLite Cloud for efficient full-text search

## Usage

To use this GitHub Action, follow these steps:

1. Create a new workflow file (e.g., `.github/workflows/docsearch.yml`) in your repository.
2. Add the following code to the workflow file:

```yaml
name: Documentation Search

on:
    push:
        branches:
            - main

jobs:
    build:
        runs-on: ubuntu-latest

        steps:
            - name: Checkout repository
              uses: actions/checkout@v4

            - name: Build SQLite Cloud database
              uses: sqlitecloud/docsearch-action@v1
              with:
                project-string: ${{ secrets.PROJECT_STRING }}
                path: path/to/your/docs
                database: my-docs-search
```

3. Make sure you have a project on SQLite Cloud. If not, sign up for an account and create one.
4. Add the Project Connection String as a secret in your repository settings, named `PROJECT_STRING`.
5. Create a database in your SQLite Cloud project and write its name in the `database` input of the action.
6. Customize these inputs according to your needs.
    * if the `path` input isn't specified the workflow will search for every .md or .mdx file recursively from the root folder.
    * `strip-html`: Set this input to `true` if you want to remove the html elements.
    * `strip-jsx`: Set this input to `true` if you want to remove the jsx elements.
    * `strip-md-titles`: Set this input to `true` if you want to remove the markdown titles to avoid redundancy in the search.
    * `strip-astro-header`: Set this input to `true` if you want to remove the Astro header from every file.
7. Commit and push the workflow file to your repository.


Now, whenever you push changes to the `main` branch, the GitHub Action will automatically update the table for your documentation website's full-text search using SQLite Cloud.

# Extra

You can also use our docbuilder from the `src` folder locally! Just compile it with your preferred compiler (don't forget to link libraries) and run it without any arguments, it will show you instructions on how to use it! By running it locally you can choose between printing sql to a file or building an SQLite  database file.

For more information and advanced configuration options, please refer to this article [SQLite Cloud Blog](https://sqlitecloud.io).
