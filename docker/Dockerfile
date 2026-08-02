FROM node:18-alpine

WORKDIR /app

COPY package.json ./
RUN npm install

COPY mock-server.js ./

EXPOSE 3000

CMD ["node", "mock-server.js"]
