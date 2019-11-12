pipeline {
    agent any

    environment {
        BLENDER_ROOT = '/var/lib/jenkins/blender'
    }

    options {
        buildDiscarder(logRotator(numToKeepStr: '3', artifactNumToKeepStr: '3'))
    }

    stages {

        stage('Setup') {
            steps {
                sh '/opt/bitnami/git/bin/git submodule update --init --recursive'
                sh 'make update'
            }
        }

        stage('Build') {
            steps {
                sh 'cd .. && ./blender/build_files/build_environment/install_deps.sh'
                sh 'make full'
            }
            post {
                always {
                    archiveArtifacts '../build_linux/bin/blender'
                }
            }
        }
    }

    post {
        always {
            deleteDir()
        }
    }
}
